#pragma once

#ifdef _WIN32
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

#include <assert.h>
#include <string.h>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <list>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hermes {

class signal {
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

static unsigned int const MAX_CONN = 100;

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
			::close(sv_[0]);
    }

    if (sv_[1] != NOTSOCK) {
			::close(sv_[1]);
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
      throw std::logic_error("Use the member function 'bind' before using 'listen' to mark the socket as passive.");
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
      throw std::logic_error("Use the member functions 'bind' then 'listen' before accepting a new connection");
    }

    socklen_t size = sizeof(client);
    int new_fd = ::accept(fd_, (struct sockaddr *)&client, &size);

    if (new_fd == NOTSOCK) {
      throw std::runtime_error("accept() failed.");
    }

    int res = getnameinfo((struct sockaddr *)&client, size, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

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
      throw std::logic_error("You cannot connect to a remote server, a socket set for a server application.");
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
      throw std::logic_error("Use the member function 'connect' to connect to a remote server before sending data.");
    }

    int res = ::send(fd_, message.data(), message_len, 0);

    if (res == -1) {
      throw std::runtime_error("send() failed.");
    }

    return res;
  }

  std::vector<char> receive(std::size_t size_to_read = internal::BUFFER_SIZE) {
    if (fd_ == NOTSOCK) {
      throw std::logic_error("Use the member function 'connect' to connect to a remote server before receiving data.");
    }

    std::vector<char> buffer(size_to_read, 0);

    int bytes_read = ::recv(fd_, const_cast<char *>(buffer.data()), size_to_read, 0);

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
        throw std::runtime_error("tcp socket close() failed.");
      }

      fd_ = -1;
      bound_ = 0;
    }
  }

 private:
  void create_socket(const std::string &host, unsigned int port) {
    if (fd_ != NOTSOCK) {
      return;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    auto service = std::to_string(port);

    int res = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &info_);

    if (res != 0) {
      throw std::runtime_error("getaddrinfo() failed.");
    }

    for (auto p = info_; p != NULL; p = p->ai_next) {
      if ((fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				continue;
			}

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
class socket {
 public:
  socket(void) : bound_(false), fd_(NOTSOCK), host_(""), port_(0) {}

  bool operator==(const socket &s) const { return fd_ == s.fd(); }

	~socket(void) { close(); }

 public:
  void init_datagram_socket(const std::string &host, unsigned int port, bool broadcasting) {
    if (!broadcasting) {
      create_socket(host, port);
    } else {
      create_broadcaster(host, port);
    }
  }

  std::size_t sendto(const std::string &str) {
    return sendto(std::vector<char>(str.begin(), str.end()), str.size());
  }

  std::size_t sendto(const std::vector<char> &data, std::size_t size) {
    if (fd_ == NOTSOCK) {
      throw std::logic_error("Datagram socket not Initialized.");
    }

    int res = ::sendto(fd_, data.data(), size, 0, info_->ai_addr, info_->ai_addrlen);

    if (res == -1) {
      throw std::runtime_error("sendto() failed.");
    }

    return res;
  }

  std::size_t broadcast(const std::string &str) {
    return broadcast(std::vector<char>(str.begin(), str.end()), str.size());
  }

  std::size_t broadcast(const std::vector<char> &data, std::size_t size) {
    if (fd_ == NOTSOCK) {
      throw std::logic_error("Datagram socket not initialized.");
    }

    int res = ::sendto(fd_, data.data(), size, 0, (struct sockaddr *)&broadcast_info_, sizeof(broadcast_info_));

    if (res == -1) {
      throw std::runtime_error("sendto() failed.");
    }

    return res;
  }

  void bind(const std::string &host, unsigned int port) {
    if (bound()) {
      throw std::logic_error("Address already assigned to the socket.");
    }

    create_socket(host, port);
    assert(info_ && info_->ai_addr && info_->ai_addrlen);
    if (::bind(fd_, info_->ai_addr, info_->ai_addrlen) == -1) {
      throw std::runtime_error("sendto() failed.");
    }

    bound_ = true;
  }

  std::size_t recvfrom(std::vector<char> &incoming) {
    if (!bound()) {
      throw std::logic_error("You must bind the socket first.");
    }

    socklen_t len = sizeof(source_info_);
    int res = ::recvfrom(fd_, incoming.data(), internal::BUFFER_SIZE - 1, 0, (struct sockaddr *)&source_info_, &len);

    if (res == -1) {
      throw std::runtime_error("recvfrom() failed.");
    }

    return res;
  }

  void close(void) {
    if (fd_ != NOTSOCK) {
      if (::close(fd_) == -1) {
        throw std::runtime_error("udp socket close() failed.");
      }

      fd_ = -1;
      bound_ = 0;
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
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    auto service = std::to_string(port);
    int status = ::getaddrinfo(!host.compare("") ? NULL : host.c_str(), service.c_str(), &hints, &info_);

    if (status != 0) {
      throw std::runtime_error("getaddrinfo() failed.");
    }

    for (auto p = info_; p != NULL; p = p->ai_next) {
      if ((fd_ = ::socket(p->ai_family, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        continue;
      }

      info_ = p;
      break;
    }

    if (fd_ == NOTSOCK) {
      throw std::runtime_error("getaddrinfo() failed.");
    }

    host_ = host;
    port_ = port;
  }

  void create_broadcaster(const std::string &host, unsigned int port) {
    if (fd_ != NOTSOCK) {
      return;
    }

    int b = 1;
    struct hostent *hostent;

    if ((hostent = ::gethostbyname(host.c_str())) == NULL) {
      throw std::runtime_error("gethostbyname() failed.");
    }

    if ((fd_ = ::socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      throw std::runtime_error("socket() failed.");
    }

    if (::setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &b, sizeof(int)) == -1) {
      throw std::runtime_error("setsockopt() failed.");
    }

    host_ = host;
    port_ = port;
    broadcast_info_.sin_family = AF_INET;
    broadcast_info_.sin_port = htons(port_);
    broadcast_info_.sin_addr = *((struct in_addr *)hostent->h_addr);
    ::memset(broadcast_info_.sin_zero, '\0', sizeof(broadcast_info_.sin_zero));
  }

 public:
  bool bound(void) const { return bound_; }

  int fd(void) const { return fd_; }

  const std::string &host(void) const { return host_; }

  unsigned int port(void) const { return port_; }

 private:
  bool bound_;

  struct sockaddr_in broadcast_info_;

  int fd_;

  struct addrinfo *info_;

  std::string host_;

  unsigned int port_;

  struct sockaddr_storage source_info_;
};
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

  ~io_service(void) {
    stop_ = 1;

    socket_pair_.write();
    if (master_.joinable()) {
      master_.join();
    }
    slaves_.stop();

    try {
      socket_pair_.close();
    } catch (const std::exception &) {
    }
  }

 private:
  void init(void) {
    try {
      socket_pair_.init();
    } catch (const std::exception &) {
    }

    master_ = std::thread([this] {
      while (!stop_) {
        sync();
        if (poll(const_cast<struct pollfd *>(poll_structs_.data()), poll_structs_.size(), TIMEOUT) > 0) {
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
        return;
      }

      auto &sub = it->second;
      sub.on_read_ = 0;

      if (sub.unsub_ && !sub.on_read_) {
        subs_.erase(it);
        condvar_.notify_all();
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
        return;
      }

      auto &sub = it->second;
      sub.on_write_ = 0;

      if (sub.unsub_ && !sub.on_write_) {
        subs_.erase(it);
        condvar_.notify_all();
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
      if (result.revents & POLLIN && sub.read_callback_ && !sub.on_read_) {
        process_read_handler(result.fd, sub);
      }

      if (result.revents & POLLOUT && sub.write_callback_ && !sub.on_write_) {
        process_write_handler(result.fd, sub);
      }

      if (sub.unsub_ && !sub.on_read_ && !sub.on_write_) {
        subs_.erase(subs_.find(result.fd));
        condvar_.notify_all();
      }
    }
  }

  void sync(void) {
    std::lock_guard<std::mutex> lock(mutex_);

    poll_structs_.clear();
    poll_structs_.push_back({socket_pair_.get_read_fd(), POLLIN, 0});
    for (auto &sub : subs_) {
      if (sub.second.read_callback_ && !sub.second.on_read_) {
        poll_structs_.push_back({sub.first, POLLIN, 0});
      }

      if (sub.second.write_callback_ && !sub.second.on_write_) {
        poll_structs_.push_back({sub.first, POLLOUT, 0});
      }

      if (sub.second.unsub_) {
        poll_structs_.push_back({sub.first, POLLIN | POLLOUT, 0});
      }
    }
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
      subs_.erase(sub);
      condvar_.notify_all();
    }

    socket_pair_.write();
  }

  template <typename T>
  void subscribe(const T &socket) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto &sub = subs_[socket.fd()];
    (void)sub;
    socket_pair_.write();
  }

  template <typename T>
  void wait_for_unsubscription(const T &socket) {
    std::unique_lock<std::mutex> lock(mutex_);
    condvar_.wait(lock, [&]() { return subs_.find(socket.fd()) == subs_.end(); });
  }

 private:
  std::condition_variable condvar_;

  std::vector<struct pollfd> poll_structs_;

  std::thread master_;

  std::mutex mutex_;

  thread_pool slaves_;

  socket_pair socket_pair_;

  std::atomic<char> stop_ = ATOMIC_VAR_INIT(0);

  std::unordered_map<int, sub> subs_;
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

class client final : internal::_no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  client(void) { io_service_ = internal::get_io_service(-1); }

  explicit client(socket &&socket) : io_service_(internal::get_io_service(-1)), socket_(std::move(socket)) {
    is_connected_ = true;
    io_service_->subscribe<tcp::socket>(socket_);
  }

  ~client(void) { disconnect(); }

 public:
  typedef std::function<void(bool &, std::vector<char> &)> async_read_callback_t;

  typedef std::function<void(bool &, std::size_t &)> async_write_callback_t;

 private:
  async_read_callback_t _receive(bool &success, std::vector<char> &buffer) {
    std::lock_guard<std::mutex> lock(read_requests_mutex_);

    if (read_requests_.empty()) {
      return nullptr;
    }

    const auto &request = read_requests_.front();
    auto callback = request.second;

    try {
      buffer = socket_.receive(request.first);
      success = true;
    } catch (const std::exception &) {
      success = false;
    }

    read_requests_.pop();

    if (read_requests_.empty()) {
      io_service_->on_read<tcp::socket>(socket_, nullptr);
    }

    return callback;
  }

  async_write_callback_t _send(bool &success, std::size_t &size) {
    std::lock_guard<std::mutex> lock(write_requests_mutex_);

    if (write_requests_.empty()) {
      return nullptr;
    }

    const auto &request = write_requests_.front();
    auto callback = request.second;

    try {
      size = socket_.send(request.first, request.first.size());
      success = true;
    } catch (const std::exception &) {
      success = false;
    }

    write_requests_.pop();

    if (write_requests_.empty()) {
      io_service_->on_write<tcp::socket>(socket_, nullptr);
    }

    return callback;
  }

  void on_read(int) {
    bool success;
    std::vector<char> buffer;
    auto callback = _receive(success, buffer);

    if (!success) {
      disconnect();
    }

    if (callback) {
      callback(success, buffer);
    }
  }

  void on_write(int) {
    bool success;
    std::size_t size;
    auto callback = _send(success, size);

    if (!success) {
      disconnect();
    }

    if (callback) {
      callback(success, size);
    }
  }

 public:
  void async_read(const std::size_t &size, const async_read_callback_t &callback) {
    std::lock_guard<std::mutex> lock(read_requests_mutex_);

    if (is_connected()) {
      io_service_->on_read<tcp::socket>(socket_, std::bind(&client::on_read, this, std::placeholders::_1));
      read_requests_.push(std::make_pair(size, callback));
    } else {
      throw std::logic_error("hermes::network::tcp::client is not connected.");
    }
  }

  void async_write(const std::string &str, const async_write_callback_t &callback) {
    async_write(std::vector<char>(str.begin(), str.end()), callback);
  }

  void async_write(const std::vector<char> &data, const async_write_callback_t &callback) {
    std::lock_guard<std::mutex> lock(write_requests_mutex_);

    if (is_connected()) {
      io_service_->on_write<tcp::socket>(socket_, std::bind(&client::on_write, this, std::placeholders::_1));
      write_requests_.push(std::make_pair(data, callback));
    } else {
      throw std::logic_error("hermes::network::tcp::client is not connected.");
    }
  }

  void connect(const std::string &host, unsigned int port) {
    if (is_connected()) {
      throw std::logic_error("hermes::network::tcp::client is already connected.");
    }

    try {
      socket_.connect(host, port);
      io_service_->subscribe<tcp::socket>(socket_);
    } catch (const std::exception &e) {
      socket_.close();
      throw e;
    }

    is_connected_ = true;
  }

  void disconnect() {
    if (!is_connected()) {
      is_connected_ = false;
      {
	std::lock_guard<std::mutex> lock(read_requests_mutex_);
	std::queue<std::pair<std::size_t, async_read_callback_t> > rempty;
	std::swap(read_requests_, rempty);
      }

      {
        std::lock_guard<std::mutex> lock(write_requests_mutex_);
	std::queue<std::pair<std::vector<char>, async_write_callback_t> > wempty;
        std::swap(write_requests_, wempty);
      }

      io_service_->unsubscribe<tcp::socket>(socket_);
      io_service_->wait_for_unsubscription<tcp::socket>(socket_);
      socket_.close();
    }
  }

 public:
  const std::string &host(void) const { return socket_.host(); }

  const std::shared_ptr<internal::io_service> &io_service(void) const { return io_service_; }

  bool is_connected(void) const { return is_connected_; }

  unsigned int port(void) const { return socket_.port(); }

  const tcp::socket &get_socket(void) const { return socket_; }

 private:
  std::shared_ptr<internal::io_service> io_service_;

  std::atomic<bool> is_connected_ = ATOMIC_VAR_INIT(false);

  tcp::socket socket_;

  std::queue<std::pair<std::size_t, async_read_callback_t> > read_requests_;

  std::mutex read_requests_mutex_;

  std::queue<std::pair<std::vector<char>, async_write_callback_t>> write_requests_;

  std::mutex write_requests_mutex_;
};

class server final : internal::_no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  server(void) : io_service_(internal::get_io_service(-1)), conn_callback_(nullptr) {}

  ~server(void) { stop(); }

 private:
  void on_connection_available(int) {
    try {
      auto client = std::make_shared<tcp::client>(socket_.accept());
      conn_callback_(client);
      clients_.push_back(client);
    } catch (const std::exception &) {
      stop();
    }
  }

 public:
  typedef std::function<void(const std::shared_ptr<client> &)> connection_callback_t;

  void on_connection(const connection_callback_t &callback) {
    if (!callback) {
      throw std::invalid_argument(
          "hermes::network::tcp::server::on_connection: Expected const "
          "std::function<void(const "
          "std::shared_ptr<hermes::network::tcp::client> &)> &.");
    }

    conn_callback_ = callback;
  }

  void run(const std::string &host, unsigned int port, unsigned int max_conn = internal::MAX_CONN) {
    if (is_running()) {
      throw std::logic_error("hermes::network::tcp::server is already running.");
    }

    if (!conn_callback_) {
      throw std::logic_error(
          "hermes::network::tcp::server: You must set a connection callback "
          "before running the server. See "
          "hermes::network::tcp::server::on_connection.");
    }

    socket_.bind(host, port);
    socket_.listen(max_conn);
    io_service_->subscribe<tcp::socket>(socket_);
    io_service_->on_read<tcp::socket>(socket_, std::bind(&server::on_connection_available, this, std::placeholders::_1));
    is_running_ = 1;
  }

  void stop() {
    if (is_running()) {
      is_running_ = 0;
      io_service_->unsubscribe<tcp::socket>(socket_);
      io_service_->wait_for_unsubscription<tcp::socket>(socket_);
      socket_.close();

      std::lock_guard<std::mutex> lock(mutex_);
      for (auto &client : clients_) {
	client->disconnect();
      }

     clients_.clear();
    }
  }

 public:
  const std::list<std::shared_ptr<client> > &clients(void) const { return clients_; }

  const std::shared_ptr<internal::io_service> &io_service(void) const { return io_service_; }

  bool is_running(void) const { return is_running_ == 1; }

  const tcp::socket &get_socket(void) const { return socket_; }

 private:
  std::list<std::shared_ptr<client> > clients_;

  std::shared_ptr<internal::io_service> io_service_;

  std::atomic<char> is_running_ = ATOMIC_VAR_INIT(false);

  std::mutex mutex_;

  tcp::socket socket_;

  connection_callback_t conn_callback_;
};

#endif
}  // namespace tcp

namespace udp {
#ifdef _WIN32
#else

class client final : internal::_no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  client(void) : broadcast_mode_(false), io_service_(internal::get_io_service(-1)) {}

  ~client(void) { }

 public:
  typedef std::function<void(int)> async_send_callback_t;

 public:
  bool broadcast_mode_enabled(void) const { return broadcast_mode_; }

  const udp::socket &get_socket(void) const { return socket_; }

 private:
  void on_send(int) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (send_requests_.empty()) {
      return;
    }

    int result = -1;
    auto request = send_requests_.front();
    auto buffer = request.first;
    auto callback = request.second;

    try {
      if (broadcast_mode_) {
        result = socket_.broadcast(buffer, buffer.size());
      } else {
        result = socket_.sendto(buffer, buffer.size());
      }
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    send_requests_.pop();

    if (callback) {
      callback(result);
    }
  }

 public:
  void init(const std::string host, unsigned int port, bool broadcast_mode) {
    socket_.init_datagram_socket(host, port, broadcast_mode);
    broadcast_mode_ = broadcast_mode;
    io_service_->subscribe<udp::socket>(socket_);
  }

  void async_send(const std::string &str, const async_send_callback_t &callback) {
    async_send(std::vector<char>(str.begin(), str.end()), callback);
  }

  void async_send(const std::vector<char> &data, const async_send_callback_t &callback) {
    if (broadcast_mode_enabled()) {
      throw std::logic_error("hermes::network::udp::client: Broadcast mode enabled. Use 'async_broadcast'.");
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (callback) {
      send_requests_.push(std::make_pair(data, callback));
      io_service_->on_write<udp::socket>(
      socket_, std::bind(&client::on_send, this, std::placeholders::_1));
    } else {
      throw std::invalid_argument("hermes::network::udp::client: You must provide a callback.");
    }
  }

  void async_broadcast(const std::string &str, const async_send_callback_t &callback) {
    async_broadcast(std::vector<char>(str.begin(), str.end()), callback);
  }

  void async_broadcast(const std::vector<char> &data, const async_send_callback_t &callback) {
    if (!broadcast_mode_enabled()) {
      throw std::logic_error("hermes::network::udp::client: Broadcast mode disabled. Use 'async_send'.");
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (callback) {
      send_requests_.push(std::make_pair(data, callback));
      io_service_->on_write<udp::socket>(
      socket_, std::bind(&client::on_send, this, std::placeholders::_1));
    } else {
      throw std::invalid_argument("hermes::network::udp::client: You must provide a callback");
    }
  }

  void stop(void) {
    broadcast_mode_ = false;
    io_service_->unsubscribe<udp::socket>(socket_);
    io_service_->wait_for_unsubscription<udp::socket>(socket_);
    socket_.close();
  }

 private:
  std::atomic<bool> broadcast_mode_ = ATOMIC_VAR_INIT(false);

  std::shared_ptr<internal::io_service> io_service_;

  std::mutex mutex_;

  std::queue<std::pair<std::vector<char>, async_send_callback_t>> send_requests_;

  udp::socket socket_;
};

class server final : internal::_no_default_ctor_cpy_ctor_mv_ctor_assign_op_ {
 public:
  server(void): io_service_(internal::get_io_service(-1)) {}

  ~server(void) { stop(); }

 public:
  typedef std::function<void(std::vector<char>, int)> async_receive_callback_t;

 private:
  void on_read(int) {
    std::lock_guard<std::mutex> lock(mutex_);

    int result = -1;
    std::vector<char> buffer;

    buffer.reserve(internal::BUFFER_SIZE);

    try {
      result = socket_.recvfrom(buffer);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    if (callback_) {
      callback_(std::move(buffer), result);
    }
  }

 public:
  void async_recvfrom(const async_receive_callback_t &callback) {
    if (!socket_.bound()) {
      throw std::logic_error("hermes::network::udp::server: You need to bind the server before.");
    }

    if (callback) {
      callback_ = callback;
    } else {
      throw std::invalid_argument("hermes::network::udp::server: You must provide a callback.");
    }
  }

  void bind(const std::string &host, unsigned int port) {
    if (is_running()) {
      throw std::logic_error("hermes::network::udp::server is already running.");
    }

    socket_.bind(host, port);
    io_service_->subscribe<udp::socket>(socket_);
    io_service_->on_read<udp::socket>(
    socket_, std::bind(&server::on_read, this, std::placeholders::_1));
    is_running_ = 1;
  }

  void stop() {
    if (is_running()) {
      is_running_ = 0;
      io_service_->unsubscribe<udp::socket>(socket_);
      io_service_->wait_for_unsubscription<udp::socket>(socket_);
      socket_.close();
    }
  }

 public:
  const std::shared_ptr<internal::io_service> &io_service(void) const { return io_service_; }

  bool is_running(void) const { return is_running_ == 1; }

  const udp::socket &get_socket(void) const { return socket_; }

 private:
  async_receive_callback_t callback_;

  std::shared_ptr<internal::io_service> io_service_;

  std::atomic<char> is_running_ = ATOMIC_VAR_INIT(false);

  std::mutex mutex_;

  udp::socket socket_;
};
#endif
}  // namespace udp

}  // namespace network

}  // namespace hermes
