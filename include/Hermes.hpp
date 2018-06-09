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
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace hermes {

namespace internal {

#ifdef _WIN32
#define NOTSOCK INVALID_SOCKET
#else
#define NOTSOCK -1
#endif

static unsigned int const BUFFER_SIZE = 4096;

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
  typedef std::function<void()> task;

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
    if (stop_ & 1) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      stop_ ^= 1;
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

    condvar_.wait(lock, [&] { return stop_ & 1 || !tasks_.empty(); });

    if (tasks_.empty()) {
      return nullptr;
    }

    auto tsk = std::move(tasks_.front());
    tasks_.pop();
    return tsk;
  }

  void routine(void) {
    for (;;) {
      if (stop_ & 1) {
        return;
      }

      auto tsk = assign_task();

      if (tsk) {
        try {
          tsk();
        } catch (const std::exception &e) {
          std::cerr << e.what() << '\n';
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
  bool bound(void) const { return bound_ & 1; }

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

    bound_ ^= 1;
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

}  // namespace hermes