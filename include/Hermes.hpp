#pragma once

#ifdef _WIN32
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hermes {

namespace internal {

#ifdef _WIN32
#define NOTSOCK INVALID_SOCKET
static int TIMEOUT = INFINITE;
#else
#define NOTSOCK -1
static int TIMEOUT = -1;
#endif

static unsigned int const BUFFER_SIZE = 4096;

static unsigned int const DEFAULT_THREAD_POOL_SIZE = 100;

class _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  _no_default_ctor_cpy_ctor_mv_ctor_assign_op_(
      const _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ &) = delete;
  _no_default_ctor_cpy_ctor_mv_ctor_assign_op_(
      const _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ &&) = delete;
  _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ &operator=(
      const _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ &) = delete;
  _no_default_ctor_cpy_ctor_mv_ctor_assign_op_() = default;
};

class signal final : _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  static std::mutex &mutex(void) {
    static std::mutex mutex;
    return mutex;
  }

  static std::condition_variable &condvar(void) {
    static std::condition_variable condvar;
    return condvar;
  }

  static void signal_handler(int) { condvar().notify_all(); }

  static void wait_for(int signal_number) {
    ::signal(signal_number, &signal::signal_handler);
    std::unique_lock<std::mutex> lock(mutex());
    condvar().wait(lock);
  }
};

class thread_pool final : _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  typedef std::function<void(void)> task;

  explicit thread_pool(unsigned int threads) {
    for (unsigned int i = 0; i < threads; i++) {
      threads_.push_back(std::thread(std::bind(&thread_pool::routine, this)));
    }
  }

  ~thread_pool(void) { stop(); }

 public:
  void register_task(const task &tsk) {
    if (!tsk) {
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    tasks_.push(tsk);
    condvar_.notify_one();
  }

  void stop(void) {
    if (stop_) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      stop_ = 1;
    }

    condvar_.notify_all();

    for (auto &thread : threads_) {
      thread.join();
    }

    threads_.clear();
  }

 private:
  task assign_task() {
    std::unique_lock<std::mutex> lock(mutex_);

    condvar_.wait(lock, [&] { return stop_ || !tasks_.empty(); });

    if (tasks_.empty()) {
      return nullptr;
    }

    auto tsk = std::move(tasks_.front());
    tasks_.pop();
    return tsk;
  }

  void routine(void) {
    for (;;) {
      if (stop_) {
        return;
      }

      auto tsk = assign_task();

      if (tsk) {
        try {
          tsk();
        } catch (const std::exception &) {
        }
      }
    }
  }

 private:
  std::condition_variable condvar_;

  std::mutex mutex_;

  std::atomic<char> stop_ = ATOMIC_VAR_INIT(0);

  std::queue<task> tasks_;

  std::vector<std::thread> threads_;
};

#ifdef _WIN32
#else
class socket_pair final : _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  socket_pair(void) : sv_{NOTSOCK, NOTSOCK} {}

  ~socket_pair(void) { close(); }

 public:
  void close(void) {
    if (sv_[0] != NOTSOCK) {
      if (::close(sv_[0]) == -1) {
        throw std::runtime_error("close() failed.");
      }
    }

    if (sv_[1] != NOTSOCK) {
      if (::close(sv_[1]) == -1) {
        throw std::runtime_error("close() failed.");
      }
    }
  }

  int get_read_fd(void) const { return sv_[0]; }

  int get_write_fd(void) const { return sv_[1]; }

  void init(void) {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv_) == -1) {
      throw std::runtime_error("socketpair() failed.");
    }
  }

  void read(void) {
    char buffer[internal::BUFFER_SIZE];
    ::read(sv_[0], buffer, internal::BUFFER_SIZE);
  }

  void write(void) { ::write(sv_[1], "h", 1); }

 private:
  int sv_[2];
};
#endif

}  // namespace internal

namespace network {

namespace tcp {
#ifdef _WIN32
#else
class socket {
 public:
  socket(void) : bound_(0), fd_(NOTSOCK), host_(""), port_(0) {}

  socket(int fd, const std::string &host, unsigned int port)
      : bound_(0), fd_(fd), host_(host), port_(port) {}

  socket(socket &&s)
      : bound_(s.bound() ? 1 : 0),
        fd_(s.fd()),
        host_(s.host()),
        port_(s.port()) {}

  bool operator==(const socket &s) const { return fd_ == s.fd(); }

  ~socket(void) = default;

 public:
  bool bound(void) const { return bound_; }

  int fd(void) const { return fd_; }

  const std::string &host(void) const { return host_; }

  unsigned int port(void) const { return port_; }

 public:
  //
  // server operations
  //
  void bind(const std::string &host, unsigned int port) {
    if (bound()) {
      return;
    }

    int yes = 1;
    create_socket(host, port);
    assert(info_ && info_->ai_addr && info_->ai_addrlen);

    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      throw std::runtime_error("setsockopt() failed.");
    }

    if (::bind(fd_, info_->ai_addr, info_->ai_addrlen) == -1) {
      throw std::runtime_error("bind() failed.");
    }

    bound_ = 1;
  }

  void listen(unsigned int backlog) {
    if (!bound()) {
      throw std::logic_error(
          "Use the member function 'bind' before using 'listen' to mark the "
          "socket as passive.");
    }

    if (backlog > SOMAXCONN) {
      throw std::invalid_argument(
          "Param backlog greater than SOMAXCONN.\nPlease refer to the "
          "value in /proc/sys/net/core/somaxconn. Param backlog will "
          "be truncated.\n");
    }

    if (::listen(fd_, backlog) == -1) {
      throw std::runtime_error("listen() failed.");
    }
  }

  tcp::socket accept(void) {
    char host[NI_MAXHOST];
    char port[NI_MAXSERV];
    struct sockaddr_storage client;

    if (fd_ == NOTSOCK) {
      throw std::logic_error(
          "Use the member functions 'bind' then 'listen' before "
          "accepting a new connection");
    }

    socklen_t size = sizeof(client);
    int new_fd = ::accept(fd_, (struct sockaddr *)&client, &size);

    if (new_fd == NOTSOCK) {
      throw std::runtime_error("accept() failed.");
    }

    int res = getnameinfo((struct sockaddr *)&client, size, host, sizeof(host),
                          port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

    if (res != 0) {
      throw std::runtime_error("getnameinfo() failed.");
    }

    return {new_fd, std::string(host), (unsigned int)std::stoi(port)};
  }

  //
  // client operations
  //
  void connect(const std::string &host, unsigned int port) {
    if (bound()) {
      throw std::logic_error(
          "You cannot connect to a remote server, a socket set for a "
          "server application.");
    }

    create_socket(host, port);
    assert(info_ && info_->ai_addr && info_->ai_addrlen);

    if (::connect(fd_, info_->ai_addr, info_->ai_addrlen) == -1) {
      throw std::runtime_error("connect() failed.");
    }
  }

  std::size_t send(const std::string &m) {
    return send(std::vector<char>(m.begin(), m.end()), m.size());
  }

  std::size_t send(const std::vector<char> &message, std::size_t message_len) {
    if (fd_ == NOTSOCK) {
      throw std::logic_error(
          "Use the member function 'connect' to connect to a remote "
          "server before sending data.");
    }

    int res = ::send(fd_, message.data(), message_len, 0);

    if (res == -1) {
      throw std::runtime_error("send() failed.");
    }

    return res;
  }

  std::vector<char> receive(std::size_t size_to_read = internal::BUFFER_SIZE) {
    if (fd_ == NOTSOCK) {
      throw std::logic_error(
          "Use the member function 'connect' to connect to a remote "
          "server before receiving data.");
    }

    std::vector<char> buffer(size_to_read, 0);

    int bytes_read =
        ::recv(fd_, const_cast<char *>(buffer.data()), size_to_read, 0);

    switch (bytes_read) {
      case -1:
        throw std::runtime_error("recv() failed.");
        break;
      case 0:
        std::cerr << "Connection closed by peer.\n";
        fd_ = NOTSOCK;
        break;
      default:
        break;
    }

    return buffer;
  }

  void close(void) {
    if (fd_ != NOTSOCK) {
      if (::close(fd_) == -1) {
        throw std::runtime_error("close() failed.");
      }
    }
  }

 private:
  void create_socket(const std::string &host, unsigned int port) {
    if (fd_ != NOTSOCK) {
      return;
    }

    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    auto service = std::to_string(port);

    int res = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &info_);

    if (res != 0) {
      throw std::runtime_error("getaddrinfo() failed.");
    }

    for (auto p = info_; p != NULL; p = p->ai_next) {
      if ((fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        continue;

      info_ = p;
      break;
    }

    if (fd_ == NOTSOCK) {
      throw std::runtime_error("socket() failed.");
    }

    host_ = host;
    port_ = port;
  }

 private:
  char bound_;

  int fd_;

  struct addrinfo *info_;

  std::string host_;

  unsigned int port_;
};
#endif  // _WIN32
}  // namespace tcp

namespace udp {
#ifdef _WIN32
#else
#endif  // _WIN32
}  // namespace udp

}  // namespace network

namespace internal {

#ifdef _WIN32
#else
#include <poll.h>
class io_service final : _no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  typedef std::function<void(int)> callback;

  struct sub {
    sub(void) : read_callback_(nullptr), write_callback_(nullptr) {}

    callback read_callback_;
    std::atomic<char> on_read_ = ATOMIC_VAR_INIT(0);

    callback write_callback_;
    std::atomic<char> on_write_ = ATOMIC_VAR_INIT(0);

    std::atomic<char> unsub_ = ATOMIC_VAR_INIT(0);
  };

 public:
  io_service(void) : slaves_(DEFAULT_THREAD_POOL_SIZE) { init(); }

  io_service(unsigned int slaves) : slaves_(slaves) { init(); }

  ~io_service(void) { stop(); }

 private:
  void init(void) {
    try {
      socket_pair_.init();
    } catch (const std::exception &) {
    }

    master_ = std::thread([this] {

      while (!stop_) {
        sync();
        if (poll(const_cast<struct pollfd *>(poll_structs_.data()),
                 poll_structs_.size(), TIMEOUT) > 0) {
          process();
        }
      }

    });
  }

  void process_read_handler(int fd, sub &sub) {
    sub.on_read_ = 1;
    auto callback = sub.read_callback_;
    slaves_.register_task([=] {
      callback(fd);

      std::lock_guard<std::mutex> lock(mutex_);
      auto it = subs_.find(fd);

      if (it == subs_.end()) {
        std::cout << "debug 1\n";
        return;
      }

      auto &sub = it->second;
      sub.on_read_ = 0;

      if (sub.unsub_ && !sub.on_read_) {
        std::cout << "process read handler sub erase\n";
        subs_.erase(it);
      }

      socket_pair_.write();
    });
  }

  void process_write_handler(int fd, sub &sub) {
    sub.on_write_ = 1;
    auto callback = sub.write_callback_;

    slaves_.register_task([=] {
      callback(fd);

      std::lock_guard<std::mutex> lock(mutex_);
      auto it = subs_.find(fd);

      if (it == subs_.end()) {
        std::cout << "debug 2\n";
        return;
      }

      auto &sub = it->second;
      sub.on_write_ = 0;

      if (sub.unsub_ && !sub.on_write_) {
        std::cout << "process write handler sub erase\n";
        subs_.erase(it);
      }

      socket_pair_.write();
    });
  }

  void process(void) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto &result : poll_structs_) {
      if (result.fd == socket_pair_.get_read_fd() && result.revents & POLLIN) {
        socket_pair_.read();
        continue;
      }

      if (subs_.find(result.fd) == subs_.end()) {
        continue;
      }

      auto &sub = subs_.find(result.fd)->second;

      auto pollin = result.revents & POLLIN;
      auto pollout = result.revents & POLLOUT;

      std::cout << "process event on fd " << result.fd << " POLLIN: " << pollin
                << " POLLOUT: " << pollout << std::endl;

      if (result.revents & POLLIN && sub.read_callback_ && !sub.on_read_) {
        process_read_handler(result.fd, sub);
      }

      if (result.revents & POLLOUT && sub.write_callback_ && !sub.on_write_) {
        process_write_handler(result.fd, sub);
      }

      if ((sub.unsub_) && !(sub.on_read_) && !(sub.on_write_)) {
        std::cout << "unsubscription for " << result.fd << std::endl;
        subs_.erase(subs_.find(result.fd));
      }
    }
  }

  void stop(void) {
    if (stop_) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      stop_ = 1;
      socket_pair_.write();
    }

    // if (master_.joinable()) {
    //   master_.join();
    // }
    while (!master_.joinable())
      ;
    master_.join();

    slaves_.stop();

    try {
      socket_pair_.close();
    } catch (const std::exception &) {
    }
  }

  void sync(void) {
    std::lock_guard<std::mutex> lock(mutex_);

    poll_structs_.clear();
    for (auto &sub : subs_) {
      auto fd = sub.first;
      auto unsub = sub.second.unsub_ == 1;
      auto onread = sub.second.on_read_ == 1;
      auto onwrite = sub.second.on_write_ == 1;

      std::cout << "io_service::sync fd " << fd << " unsub " << unsub
                << " onread " << onread << " onwrite " << onwrite << std::endl;

      // if (sub.second.unsub_) {
      //   poll_structs_.push_back({sub.first, POLLIN, 0});
      // }

      if (sub.second.read_callback_ && !sub.second.on_read_) {
        poll_structs_.push_back({sub.first, POLLIN, 0});
      }

      if (sub.second.write_callback_ && !sub.second.on_write_) {
        poll_structs_.push_back({sub.first, POLLOUT, 0});
      }
    }

    poll_structs_.push_back({socket_pair_.get_read_fd(), POLLIN, 0});
  }

 public:
  template <typename T>
  void on_read(const T &socket, callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto &sub = subs_[socket.fd()];
    sub.read_callback_ = cb;
    socket_pair_.write();
  }

  template <typename T>
  void on_write(const T &socket, callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto &sub = subs_[socket.fd()];
    sub.write_callback_ = cb;
    socket_pair_.write();
  }

  template <typename T>
  void unsubscribe(const T &socket) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sub = subs_.find(socket.fd());

    if (sub == subs_.end()) {
      return;
    }

    if (sub->second.on_read_ || sub->second.on_write_) {
      sub->second.unsub_ = 1;
    } else {
      std::cout << "unsubscription for " << sub->first << std::endl;
      subs_.erase(sub);
    }

    socket_pair_.write();
  }

  template <typename T>
  void subscribe(const T &socket) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto &sub = subs_[socket.fd()];
    auto u = sub.unsub_ == 1;
    auto r = sub.on_read_ == 1;
    auto w = sub.on_write_ == 1;
    auto rc = sub.read_callback_ ? true : false;
    auto wc = sub.write_callback_ ? true : false;
    std::cout << "subscribe fd: " << socket.fd() << " unsub " << u << " onread "
              << r << " onwrite " << w << " rd_cb " << rc << " wr_cb " << wc
              << std::endl;
    (void)sub;
    socket_pair_.write();
  }

 private:
  std::unordered_map<int, sub> subs_;

  std::thread master_;

  std::mutex mutex_;

  std::vector<struct pollfd> poll_structs_;

  thread_pool slaves_;

  std::atomic<char> stop_ = ATOMIC_VAR_INIT(0);

  socket_pair socket_pair_;
};
#endif

namespace {
static std::shared_ptr<io_service> g_io_service = nullptr;
}  // namespace

void set_io_service(const std::shared_ptr<io_service> &mpx) {
  if (g_io_service) {
    g_io_service.reset();
  }

  g_io_service = mpx;
}

const std::shared_ptr<io_service> &get_io_service(int slaves) {
  if (!g_io_service) {
    if (slaves <= 0) {
      g_io_service = std::make_shared<io_service>();
    } else {
      g_io_service = std::make_shared<io_service>(slaves);
    }
  }

  return g_io_service;
}

}  // namespace internal

namespace network {
namespace tcp {
#ifdef _WIN32
#else
#endif
}  // namespace tcp

namespace udp {
#ifdef _WIN32
#else
#endif
}  // namespace udp

}  // namespace network

}  // namespace hermes