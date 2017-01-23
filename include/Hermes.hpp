#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#define UNUSED(x) __pragma(warning(suppress : 4100)) x
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

#include <atomic>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hermes {

namespace tools {
// Feel free to modify any value of the following variables to fit to your
// needs:
//
// - TIMEOUT
// - BACKLOG
// - BUFFER_SIZE
// - THREADS_NBR

// Default timeout value in milliseconds.
// It defines the minimum number of milliseconds that the poller will block.
// Set to infinite (-1 on unix system) by default in order to allow the poller
// to wait indefinitely for an event.
#ifdef _WIN32
static INT TIMEOUT = INFINITE;
#else
static int TIMEOUT = -1;
#endif  // _WIN32

// Default size for the maximum length to which the queue for pending
// connections may grow.
static unsigned int const BACKLOG = 100;
// Default size used for buffers.
static unsigned int const BUFFER_SIZE = 8096;
// Default number of concurrent threads supported by the system.
static unsigned int const THREADS_NBR = std::thread::hardware_concurrency();

//
// _default_signatures_ class represents the default signatures for the
// following:
//  - Default constructor.
//  - Copy constructor.
//  - Move constructor.
//  - Assignment operator.
//
class _default_signatures_ {
 public:
  _default_signatures_(const _default_signatures_ &) = delete;

  _default_signatures_(const _default_signatures_ &&) = delete;

  _default_signatures_ &operator=(const _default_signatures_ &) = delete;

  _default_signatures_() = default;
};

//
// When construct, the 'error' class stores the value of 'errno'. The store
// value of 'errno' is retrievable using the 'get_error()' member function.
//
// Furthermore, 'error' allows to throw an exception of a specified type and
// format the error message. The type of exception to be thrown is represented
// following:
//
//    enum type {
//      NONE = 0,    // default type.
//      ARGS = 1,    // Should throw an exception of type: std::invalid_argument
//      LOGIC = 2,   // Should throw an exception of type: std::logic_error
//      RUNTIME = 3  // Should throw an exception of type: std::runtime_error
//    };
//
class error final : _default_signatures_ {
 public:
  error() : errno_(errno) {}

  ~error() = default;

  // Represents the type of exception.
  enum type { NONE = 0, ARGS = 1, LOGIC = 2, RUNTIME = 3 };

 public:
  // Returns the stored value of 'errno'.
  int get_error(void) const { return errno_; }

  // Format the error message.
  //
  // @static:
  //  Member function specified as 'static', it can be used without instanciate
  //  an 'error' object.
  //
  // @params:
  //    - [in] a reference on a const string containing the name of the function
  //    where the error occurred.
  //    - [line] int representing the number of the line where the error is
  //    reported.
  //    - [message] a reference on a const string containing the message to
  //    report.
  //
  // @return value: A string reporting the error correctly formated.
  //
  static std::string format(const std::string &in, int line,
                            const std::string &message) {
    return std::string(__FILE__) + std::string(": ") + in + std::string("\n") +
           std::string(__FILE__) + std::string(":") + std::to_string(line) +
           std::string(" ") + message + std::string("\n");
  }

  // Throw an exeception of a given type and format the according message.
  //
  // @static:
  //  Member function specified as 'static', it can be used without instanciate
  //  an 'error' object.
  //
  // @params:
  //    - [type] enum type, representing the type of exception to throw.
  //    - [in] a reference on a const string containing the name of the function
  //    where the error occurred.
  //    - [line] int representing the number of the line where the error is
  //    reported.
  //    - [message] a reference on a const string containing the message to
  //    report.
  //
  static void require_throws(type type, const std::string &in, int line,
                             const std::string &message) {
    switch (type) {
      case ARGS:
        throw std::invalid_argument(format(in, line, message));
        break;
      case LOGIC:
        throw std::logic_error(format(in, line, message));
        break;
      case RUNTIME:
        throw std::runtime_error(format(in, line, message));
        break;
      default:
        break;
    }
  }

 private:
  int errno_;
};

// Convenience macro.
#define HERMES_THROW(type, in, line, message)       \
  {                                                 \
    std::cerr << message << std::endl;              \
    error::require_throws(type, in, line, message); \
  }

//
// The 'defer' class allows you to designate specified callbacks to be executed
// just before returning from the current function block. The callback required
// to construct a 'defer' object will be executed when the object is destroyed.
//
class defer final : _default_signatures_ {
 public:
  defer(const std::function<void(void)> &callback) : callback_(callback) {}

  defer(std::function<void(void)> &&callback)
      : callback_(std::move(callback)) {}

  ~defer(void) {
    if (callback_) callback_();
  }

 private:
  // The callback to execute when the 'defer' object is destroyed.
  std::function<void(void)> callback_;
};

//
// A signal wrapper, it allows to block the thread calling until the specified
// signal is caught.
//
class signal final : _default_signatures_ {
 public:
  // Returns a reference on a static mutex, used to build the unique_lock to
  // lock the thread.
  static std::mutex &mutex(void) {
    static std::mutex mutex;
    return mutex;
  }

  // Returns a reference on a static condition variable, used to block the
  // calling thread until notified to resume.
  static std::condition_variable &condvar(void) {
    static std::condition_variable condvar;
    return condvar;
  }

  // A signal handler.
  // If the registered signal is caught it will invoke this function and notify
  // the condition variable to resume.
  static void signal_handler(int) { condvar().notify_all(); }

  // Wait until the specified signal is caught.
  // The 'wait_for()' member function wait until the specified signal is caught.
  // The calling thread remains blocked until woken up by another thread
  // invoking the signal handler when the signal is caught.
  //
  // @static:
  //  Member function specified as 'static', it can be used without instanciate
  //  a 'signal' object.
  //
  // @param:
  //    - [signal_number] int representing the signal to catch.
  //
  static void wait_for(int signal_number) {
    ::signal(signal_number, &signal::signal_handler);
    std::unique_lock<std::mutex> lock(mutex());
    condvar().wait(lock);
  }
};

//
// A thread pool using threads as workers to execute concurrently jobs stored in
// the synchronized queue. The workers start working and waiting for jobs as
// soon as the 'workers' object is constructed.
//
class workers final : _default_signatures_ {
 public:
  // Constructor.
  //
  // @param:
  //  - [workers_nbr] unsigned int representing the number of concurrent threads
  //  that the constructor will create.
  //
  explicit workers(unsigned int workers_nbr = THREADS_NBR) : stop_(false) {
    // Check the number of concurrent threads supported by the system.
    if (workers_nbr > std::thread::hardware_concurrency()) {
      HERMES_THROW(error::ARGS, __PRETTY_FUNCTION__, __LINE__,
                   " error: Number of 'workers' is greater than the number of "
                   "concurrent hreads supported by the system.");
    }

    // Start the workers.
    for (unsigned int i = 0; i < workers_nbr; ++i)
      workers_.push_back(std::thread([this]() {
        // Worker routine:
        // Each worker is waiting for a new job. The first worker that can
        // process a job, removes it from the queue and executes it.

        // We loop waiting for a new job.
        while (!stop_) {
          auto job = retrieve_job();
          if (job) job();
        }

      }));
  }

  ~workers(void) { stop(); }

 public:
  // Returns true or false whether the workers are working.
  bool are_working(void) const { return !stop_; }

  // Allows the user to enqueue a new job which must be processed.
  // It will notify every threads that a job has been enqueued.
  //
  // @param:
  //    - [new_job] a reference on a const function object representing the job
  //    to enqueue.
  //
  void enqueue_job(const std::function<void(void)> &new_job) {
    if (!new_job) {
      std::cout << "WARNING: Passing nullptr instead of const "
                   "std::function<void(void)> &.\n";
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_job_queue_);
    job_queue_.push(new_job);
    condition_.notify_all();
  }

  // Stop the thread pool.
  void stop(void) {
    if (stop_) return;

    stop_ = true;
    condition_.notify_all();
    for (auto &worker : workers_) worker.join();
    workers_.clear();
  }

 private:
  // Check the job queue to know if there is a job waiting. If that is the case
  // it returns the job and removes it from the queue.
  std::function<void(void)> retrieve_job(void) {
    std::unique_lock<std::mutex> lock(mutex_job_queue_);

    condition_.wait(lock, [&] { return stop_ || !job_queue_.empty(); });

    if (job_queue_.empty()) return nullptr;

    auto job = std::move(job_queue_.front());
    job_queue_.pop();
    return job;
  }

 private:
  // Boolean to know if the workers should stop working.
  std::atomic_bool stop_;

  // Mutex to synchronize the queue.
  std::mutex mutex_job_queue_;

  // Condition variable to synchronize the threads.
  std::condition_variable condition_;

  // Thread pool.
  std::vector<std::thread> workers_;

  // Contains pending jobs.
  std::queue<std::function<void(void)>> job_queue_;
};

}  // namespace tools

namespace network {

using namespace tools;

namespace tcp {

#ifdef _WIN32

// Windows tcp socket.
// Provides blocking stream-oriented socket functionalities.
class socket final : _default_signatures_ {
 public:
  socket(void) : fd_(-1), host_(""), port_(0), has_a_name_assigned_(false) {}

  socket(SOCKET fd, const std::string &host, unsigned int port)
      : fd_(fd), host_(host), port_(port), has_a_name_assigned_(false) {}

  socket(socket &&socket)
      : fd_(std::move(socket.get_fd())),
        host_(socket.get_host()),
        port_(socket.get_port()),
        has_a_name_assigned_(false) {}

  bool operator==(const socket &socket) const { return fd_ == socket.get_fd(); }

  ~socket(void) = default;

 public:
  SOCKET get_fd(void) const { return fd_; }

  const std::string &get_host(void) const { return host_; }

  unsigned int get_port(void) const { return port_; }

  bool has_a_name_assigned(void) const { return has_a_name_assigned_; }

 public:
  void bind(const std::string &host, unsigned int port) {
    UNUSED(host);
    UNUSED(port);
  }

  void listen(unsigned int backlog) { UNUSED(backlog); }

  tcp::socket accept(void) { return {0, "", 0}; }

  void connect(const std::string &host, unsigned int port) {
    UNUSED(host);
    UNUSED(port);
  }

  INT send(const std::string &data) {
    return send(std::vector<char>(data.begin(), data.end()), data.size());
  }

  INT send(const std::vector<char> &data, INT size) {
    UNUSED(data);
    UNUSED(size);
    return 0;
  }

  std::vector<char> receive(INT size_to_read) {
    std::vector<char> buffer(size_to_read, 0);
    return buffer;
  }

  void close(void) {}

 private:
  SOCKET fd_;

  std::string host_;

  unsigned int port_;

  bool has_a_name_assigned_;
};

#else

// Unix tcp socket.
// Provides blocking stream-oriented socket functionalities.
class socket final : _default_signatures_ {
 public:
  // Basic constructor.
  socket(void) : fd_(-1), host_(""), port_(0), has_a_name_assigned_(false) {}

  // Create a socket from an existing file descriptor.
  socket(int fd, const std::string &host, unsigned int port)
      : fd_(fd), host_(host), port_(port), has_a_name_assigned_(false) {}

  // Move constructor.
  socket(socket &&socket)
      : fd_(std::move(socket.get_fd())),
        host_(socket.get_host()),
        port_(socket.get_port()),
        has_a_name_assigned_(false) {
    socket.fd_ = -1;
  }

  // Comparison operator.
  bool operator==(const socket &s) const { return fd_ == s.get_fd(); }

  ~socket(void) = default;

 public:
  // Returns the file descriptor associated to the socket.
  int get_fd(void) const { return fd_; }

  // Returns the socket address.
  const std::string &get_host(void) const { return host_; }

  // Returns the socket port.
  unsigned int get_port(void) const { return port_; }

  // Returns true or false whether the socket has a name assigned.
  bool has_a_name_assigned(void) const { return has_a_name_assigned_; }

 public:
  //
  // Server operations.
  //

  // Assign a name to the socket.
  //
  // @params:
  //    - [host] a reference on a const string representing the address to
  //    assign to the socket.
  //    - [port] unsigned int representing the port to assign to the socket.
  //
  void bind(const std::string &host, unsigned int port) {
    if (has_a_name_assigned()) {
      std::cout << "WARNING: An address is already assigned to this socket.\n";
      return;
    }

    int yes = 1;
    create_socket(host, port);
    assert(info_ && info_->ai_addr && info_->ai_addrlen);
    defer that([this] { ::freeaddrinfo(info_); });

    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: setsockopt() failed.");
    }

    if (::bind(fd_, info_->ai_addr, info_->ai_addrlen) == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: bind() failed.");
    }

    has_a_name_assigned_ = true;
  }

  // Mark the socket as a passive socket.
  //
  // @param: cf top of "Hermes.hpp"
  //
  void listen(unsigned int backlog = BACKLOG) {
    if (!has_a_name_assigned()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: Use the member function 'bind' before "
                   "using 'listen' to mark the socket as passive.");
    }

    if (backlog > SOMAXCONN) {
      std::cout << "Param backlog greater than SOMAXCONN.\nPlease refer to the";
      std::cout << " value in /proc/sys/net/core/somaxconn. Param backlog will";
      std::cout << " be truncated.\n";
    }

    if (::listen(fd_, backlog) == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: listen() failed.");
    }
  }

  // Accept a new connection.
  //
  // @return value: A TCP socket referring to the connected client.
  //
  tcp::socket accept(void) {
    char host[NI_MAXHOST];
    char port[NI_MAXSERV];
    struct sockaddr_storage client;

    if (fd_ == -1) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: Use the member functions 'bind' then "
                   "'listen' before accepting a new connection.");
    }

    socklen_t size = sizeof(client);
    int new_fd = ::accept(fd_, (struct sockaddr *)&client, &size);

    if (new_fd == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: accept() failed.");
    }

    int res = getnameinfo((struct sockaddr *)&client, size, host, sizeof(host),
                          port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

    if (res != 0) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: getnameinfo() failed.");
    }

    return {new_fd, std::string(host), (unsigned int)std::stoi(port)};
  }

  //
  // Client operations.
  //

  // Connect to a remote host.
  //
  // @params:
  //    - [host] a reference on a const string containing the address of the
  //    remote server which we want to connect.
  //    - [port] unsigned int representing the port of the remote server
  //    which we want to connect.
  //
  void connect(const std::string &host, unsigned int port) {
    if (has_a_name_assigned()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: You cannot connect to a remote server, "
                   "a socket set for a server application.");
    }

    create_socket(host, port);
    assert(info_ && info_->ai_addr && info_->ai_addrlen);
    defer that([this] { ::freeaddrinfo(info_); });

    if (::connect(fd_, info_->ai_addr, info_->ai_addrlen) == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: connect() failed.");
    }
  }

  // Send data.
  //
  // @params:
  //    - [message] a reference on a const string containing the message that we
  //    want to send.
  //
  std::size_t send(const std::string &message) {
    return send(std::vector<char>(message.begin(), message.end()),
                message.size());
  }

  // Send data.
  //
  // @params:
  //    - [message] a reference on a const vector of char containing the message
  //    to send.
  //    - [message_len] std::size_t representing the size of the message.
  //
  std::size_t send(const std::vector<char> &message, std::size_t message_len) {
    if (fd_ == -1) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: Use the member function 'connect' to "
                   "connect to a remote server before sending data.");
    }

    int res = ::send(fd_, message.data(), message_len, 0);

    if (res == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: send() failed.");
    }

    return res;
  }

  // Receive data.
  //
  // @param:
  //    - [size_to_read] std::size_t representing the number of bytes that we
  //    want to received.
  //
  // @return value:
  //   - A vector of char containing the data received.
  //
  std::vector<char> receive(std::size_t size_to_read = BUFFER_SIZE) {
    if (fd_ == -1) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: Use the member function 'connect' to "
                   "connect to a remote server before receiving data.");
    }

    std::vector<char> buffer(size_to_read, 0);

    int bytes_read =
        ::recv(fd_, const_cast<char *>(buffer.data()), size_to_read, 0);

    switch (bytes_read) {
      case -1:
        HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                     "error: recv() failed.");
        break;
      case 0:
        std::cout << "INFO: Connection closed by the remote host.\n";
        close();
        break;
      default:
        break;
    }

    return buffer;
  }

  //
  // Common operation.
  //

  // Close the file descriptor associated to the socket and reset the socket.
  void close(void) {
    if (fd_ != -1) {
      defer that([this] {
        fd_ = -1;
        host_ = "";
        port_ = 0;
        has_a_name_assigned_ = false;
      });

      if (::close(fd_) == -1) {
        HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                     "error: close() failed.");
      }
    }
  }

 private:
  // Create an endpoint for communication.
  //
  //  @params:
  //      - [host] a reference on a const string containing the address of a
  //      remote host.
  //      - [port] unsigned int representing the port of a remote host.
  //
  void create_socket(const std::string &host, unsigned int port) {
    if (fd_ != -1) return;

    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    auto service = std::to_string(port);

    int res = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &info_);

    if (res != 0) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   " error: getaddrinfo() failed.");
    }

    for (auto p = info_; p != NULL; p = p->ai_next) {
      if ((fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        continue;
      info_ = p;
      break;
    }

    if (fd_ == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   " error: socket() failed.");
    }

    host_ = host;
    port_ = port;
  }

 private:
  // file descriptor associated to the socket.
  int fd_;

  // Socket address.
  std::string host_;

  // Socket port.
  unsigned int port_;

  // Boolean to know if the socket as a name assigned.
  bool has_a_name_assigned_;

  // Network address used by the socket.
  struct addrinfo *info_;
};

#endif  // _WIN32

}  // namespace tcp

namespace udp {

#ifdef _WIN32

// Windows udp socket.
// Provides synchronous datagram-oriented socket functionality.
class socket final : _default_signatures_ {
 public:
  socket(void) : fd_(-1), host_(""), port_(0), has_a_name_assigned_(false) {}

  socket(SOCKET fd, const std::string &host, unsigned int port)
      : fd_(fd), host_(host), port_(port), has_a_name_assigned_(false) {}

  bool operator==(const socket &socket) const { return fd_ == socket.get_fd(); }

  ~socket(void) = default;

 public:
  SOCKET get_fd(void) const { return fd_; }

  const std::string &get_host(void) const { return host_; }

  unsigned int get_port(void) const { return port_; }

  bool has_a_name_assigned(void) const { return has_a_name_assigned_; }

 public:
  void init_datagram_socket(const std::string &host, unsigned int port,
                            bool broadcast_mode) {
    UNUSED(host);
    UNUSED(port);
    UNUSED(broadcast_mode);
  }

  INT sendto(const std::string &data) {
    return sendto(std::vector<char>(data.begin(), data.end()), data.size());
  }

  INT sendto(const std::vector<char> &data, INT size) {
    UNUSED(data);
    UNUSED(size);
    return 0;
  }

  INT broadcast(const std::string &data) {
    return broadcast(std::vector<char>(data.begin(), data.end()), data.size());
  }

  INT broadcast(const std::vector<char> &data, INT size) {
    UNUSED(data);
    UNUSED(size);
    return 0;
  }

  void bind(const std::string &host, unsigned int port) {
    UNUSED(host);
    UNUSED(port);
  }

  INT recvfrom(std::vector<char> &buffer) {
    UNUSED(buffer);
    return 0;
  }

  void close(void) {}

 private:
  SOCKET fd_;

  std::string host_;

  unsigned int port_;

  bool has_a_name_assigned_;
};

#else

// Unix udp socket.
// Provides synchronous datagram-oriented socket functionality.
class socket final : _default_signatures_ {
 public:
  socket(void) : fd_(-1), host_(""), port_(0), has_a_name_assigned_(false) {}

  bool operator==(const socket &s) const { return fd_ == s.get_fd(); }

  ~socket(void) = default;

 public:
  // Returns the file descriptor associated to the socket.
  int get_fd(void) const { return fd_; }

  // Returns the host associated to the socket.
  const std::string &get_host(void) const { return host_; }

  // Returns the port associated to the socket.
  unsigned int get_port(void) const { return port_; }

  // Returns true if the socket is bound, false otherwise.
  bool has_a_name_assigned(void) const { return has_a_name_assigned_; }

 public:
  //
  //  Client operations
  //

  // Initialize a datagram socket.
  //
  // @params:
  //    - [host] a reference on a const string containing the host address.
  //    - [port] unsigned int representing the port of the remote host.
  //    - [broadcasting] Boolean to know if we want to enable the broadcasting
  //    mode.
  //
  void init_datagram_socket(const std::string &host, unsigned int port,
                            bool broadcasting) {
    if (!broadcasting)
      create_socket(host, port);
    else
      create_broadcaster(host, port);
  }

  // Send data to another socket.
  //
  //  @param:
  //    - [str] a reference on a const string containing the data to send.
  //
  //  @return value:
  //    - std::size_t: bytes sent.
  //
  std::size_t sendto(const std::string &str) {
    return sendto(std::vector<char>(str.begin(), str.end()), str.size());
  }

  // Send data to another socket.
  //
  //  @params:
  //    - [data] a reference on a const vector of char containing the data to
  //    send.
  //    - [size] std::size_t resenting the size of the vector.
  //
  //  @return value:
  //    - std::size_t: bytes sent.
  //
  std::size_t sendto(const std::vector<char> &data, std::size_t size) {
    if (fd_ == -1) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: Datagram socket not Initialized.");
    }

    int res =
        ::sendto(fd_, data.data(), size, 0, info_->ai_addr, info_->ai_addrlen);

    if (res == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: sendto() failed.");
    }

    return res;
  }

  // Broadcast data.
  //
  //  @param:
  //    - [str] a reference on a const string containing the data to broadcast.
  //
  //  @return value:
  //    - std::size_t: bytes broadcasted.
  //
  std::size_t broadcast(const std::string &str) {
    return broadcast(std::vector<char>(str.begin(), str.end()), str.size());
  }

  // Broadcast data.
  //
  //  @param:
  //    - [data] a reference on a const vector of char containing the data to
  //    broadcast.
  //    - [size] std::size_t representing the size of the vector.
  //
  //  @return value:
  //    - std::size_t: bytes broadcasted.
  //
  std::size_t broadcast(const std::vector<char> &data, std::size_t size) {
    if (fd_ == -1) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: Datagram socket not Initialized.");
    }

    int res =
        ::sendto(fd_, data.data(), size, 0, (struct sockaddr *)&broadcast_info_,
                 sizeof(broadcast_info_));

    if (res == -1) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "error: sendto() failed.");
    }

    return res;
  }

  //
  // Server operations.
  //

  // Assign a name to the socket.
  //
  //  @params:
  //      - [host] a reference on a const string containing the address to
  //      assign to the socket.
  //      - [port] unsigned int representing the port to assign to the socket.
  //
  void bind(const std::string &host, unsigned int port) {
    if (has_a_name_assigned()) {
      std::cout << "WARNING: Address already assigned to the socket.\n";
      return;
    }

    create_socket(host, port);
    assert(info_ && info_->ai_addr && info_->ai_addrlen);
    defer that([this] { ::freeaddrinfo(info_); });

    if (::bind(fd_, info_->ai_addr, info_->ai_addrlen) == -1) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   " error: sendto() failed.");
    }

    has_a_name_assigned_ = true;
  }

  // Receive data from another socket.
  //
  //  @param:
  //    - [incoming] a reference on a vector of char to store the bytes
  //    received.
  //
  //  @return value:
  //    - std::size_t: bytes broadcasted.
  //
  std::size_t recvfrom(std::vector<char> &incoming) {
    if (!has_a_name_assigned()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: You must bind the socket first.");
    }

    socklen_t len;
    len = sizeof(source_info_);
    int res = ::recvfrom(fd_, incoming.data(), tools::BUFFER_SIZE - 1, 0,
                         (struct sockaddr *)&source_info_, &len);

    if (res == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: recvfrom() failed.");
    }

    return res;
  }

  //
  //  Common operations.
  //

  // Close the file descriptor associated to the socket.
  void close(void) {
    defer that([this] {
      fd_ = -1;
      host_ = "";
      port_ = 0;
      has_a_name_assigned_ = false;
    });

    if (fd_ != -1) {
      if (::close(fd_) == -1) {
        HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                     "error: close() failed.");
      }
    }
  }

 private:
  // Create an endpoint for communication with the given host/port.
  //
  //  @params:
  //      - [host] a reference on a const string containing a network address.
  //      - [port] unsigned int representing a port.
  //
  void create_socket(const std::string &host, unsigned int port) {
    if (fd_ != -1) return;

    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    auto service = std::to_string(port);

    int status = ::getaddrinfo(!host.compare("") ? NULL : host.c_str(),
                               service.c_str(), &hints, &info_);

    if (status != 0) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: getaddrinfo() failed.");
    }

    for (auto p = info_; p != NULL; p = p->ai_next) {
      if ((fd_ = ::socket(p->ai_family, SOCK_DGRAM | SOCK_CLOEXEC,
                          IPPROTO_UDP)) == -1) {
        continue;
      }

      info_ = p;
      break;
    }

    if (fd_ == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: getaddrinfo() failed.");
    }

    host_ = host;
    port_ = port;
  }

  // Create a socket and enable it for broadcasting data.
  //
  //  @params:
  //      - [host] a reference on a const string containing a network address.
  //      - [port] unsigned int representing a port.
  //&
  void create_broadcaster(const std::string &host, unsigned int port) {
    if (fd_ != -1) return;

    int b = 1;
    struct hostent *hostent;

    if ((hostent = ::gethostbyname(host.c_str())) == NULL) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: gethostbyname() failed.");
    }

    if ((fd_ = ::socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: socket() failed.");
    }

    if (::setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &b, sizeof(int)) == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   "error: setsockopt() failed.");
    }

    host_ = host;
    port_ = port;
    broadcast_info_.sin_family = AF_INET;
    broadcast_info_.sin_port = ::htons(port_);
    broadcast_info_.sin_addr = *((struct in_addr *)hostent->h_addr);
    ::memset(broadcast_info_.sin_zero, '\0', sizeof(broadcast_info_.sin_zero));
  }

 private:
  // file descriptor associated to the socket.
  int fd_;

  // Socket address.
  std::string host_;

  // Socket port.
  unsigned int port_;

  // Boolean to know if the socket is bound.
  bool has_a_name_assigned_;

  // Network address used by the socket.
  struct addrinfo *info_;

  // Connector's address information for broadcasting data.
  struct sockaddr_in broadcast_info_;

  // Information on where the data come from.
  struct sockaddr_storage source_info_;
};

#endif  // _WIN32

}  // namespace udp
}  // namespace network

#ifdef _WIN32

// Windows event model.
class event final : _default_signatures_ {
 public:
  event(void) {}
  ~event(void) = default;
};

// A Windows polling wrapper.
class poller final : _default_signatures_ {
 public:
  poller(void) {}
  ~poller(void) {}

 public:
  //
  template <typename T>
  bool has(const T &socket) {
    UNUSED(socket);
    return true;
  }

  //
  template <typename T>
  void add(const T &socket) {
    UNUSED(socket);
  }

  //
  template <typename T>
  void wait_for_read(const T &s, const std::function<void(void)> &c) {
    UNUSED(s);
    UNUSED(c);
  }

  //
  template <typename T>
  void wait_for_write(const T &s, const std::function<void(void)> &c) {
    UNUSED(s);
    UNUSED(c);
  }

  //
  template <typename T>
  void remove(const T &socket) {
    UNUSED(socket);
  }

 private:
  // A map containing:
  // @key: file descriptor (SOCKET under Windows).
  // @value: class event representing the event monitored.
  std::unordered_map<SOCKET, event> events_;
};

#else

using namespace hermes::tools;

// Unix event model.
// The 'event' object needs to be associated to a file descriptor referring to a
// socket. It allows to store callbacks which must be processed if the socket is
// ready for a reading or a writting operation.
// Furthermore, the event model allows to know if we are currently executing a
// specific callback.
// The event model owns a pollfd structure which will be used in the polling
// model. Event objects can update their own pollfd structure according the
// callbacks defined and the potential current execution of one of these
// callbacks.
class event final : _default_signatures_ {
 public:
  // Constructs a default event model.
  event(void) : unwatch_(false), pollfd_({-1, 0, 0}) {
    on_send_.running = false;
    on_send_.callback = nullptr;
    on_receive_.running = false;
    on_receive_.callback = nullptr;
  }

  ~event(void) = default;

 public:
  // Structure containing information on a callback for a specific event.
  struct event_callback_info {
    // Boolean to know if the associated callback is currently running.
    std::atomic_bool running;

    // the callback stored which will be executed.
    std::function<void(void)> callback;
  };

 public:
  // Returns true or false if there is a specific events defined.
  bool has(void) const { return pollfd_.events != 0; }

  // Updates the pollfd structure.
  //
  // Sets the value of the file descriptor in the pollfd structure.
  // The variable 'event' of the pollfd structure is set with either POLLIN if
  // we are waiting for a read operation, or POLLOUT for a read operation
  //
  //  @param:
  //    - [fd] int representing the file descriptor associated to detected
  //    event.
  //
  void update(int fd) {
    if (unwatch_ || (!on_receive_.callback && !on_send_.callback)) return;

    pollfd_.fd = fd;

    if (on_send_.callback && !on_send_.running) pollfd_.events |= POLLOUT;

    if (on_receive_.callback && !on_receive_.running) pollfd_.events |= POLLIN;
  }

  // Returns the pollfd structure.
  //
  //  @return value:
  //    - a reference on struct pollfd.
  //
  struct pollfd &get_poll_struct(void) {
    return pollfd_;
  }

  // Reset the pollfd structure.
  void reset_poll_struct(void) {
    pollfd_.fd = -1;
    pollfd_.events = 0;
    pollfd_.revents = 0;
  }

 public:
  // Boolean to know if the poller should stop monitoring this file descriptor.
  std::atomic_bool unwatch_;

  // Structure holding the file descriptor and the event to monitor as well as
  // the detected event.
  struct pollfd pollfd_;

  // Structure holding the callback to excecute if the file descriptor is ready
  // for a write operation.
  event_callback_info on_send_;

  // Structure holding the callback to execute if the file descriptor is ready
  // for a read operation.
  event_callback_info on_receive_;
};

// A Unix polling wrapper.
// Poller provides an access to 'poll()' for any given socket. The poller is
// using a thread pool as workers to execute the callback provided when the
// socket is ready for the expected event.
// Poller associates the file descriptors of the monitored sockets with 'event'
// objects to store the callbacks provided. In order to offer an asynchronous
// I/O model, we are waiting using 'poll()' for one of a set of file descriptors
// to become ready to perform an I/O operation.
// When a file descriptor is ready, a job is enqueued in the thread pool in
// order to perform the associated callback for this operation.
class poller final : _default_signatures_ {
 public:
  // Construct an empty polling model.
  poller(void) : stop_(false), socketpair_{-1, -1} {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, socketpair_) == -1) {
      HERMES_THROW(error::RUNTIME, __PRETTY_FUNCTION__, __LINE__,
                   " error: socketpair() failed.");
    }

    poll_master_ = std::thread([this]() {
      while (!stop_) {
        synchronize_events();

        if (::poll(const_cast<struct pollfd *>(poll_structs_.data()),
                   poll_structs_.size(), TIMEOUT) > 0)
          process_detected_events();
      }
    });
  }

  // Workers should stop working and the poll main thread is joined.
  ~poller(void) {
    stop_ = true;
    socketpair_write();
    poll_master_.join();
    workers_.stop();
    // close one of the connected socketpair.
    ::close(socketpair_[0]);
    // close the other connected socketpair.
    ::close(socketpair_[1]);
  }

 public:
  // Check if we are monitoring a specific socket.
  // Returns true if the socket is currently monitored.
  //
  // @param
  //    - [s] a reference on a const socket of type T.
  //
  // @return value:
  //    - boolean.
  //
  template <typename T>
  bool has(const T &s) {
    return events_.find(s.get_fd()) == events_.end() ? false : true;
  }

  // Add a socket to the polling model.
  // A socket must be added to the poller before being monitored for a specific
  // event.
  //
  //  @param:
  //    - [socket] a reference on a const socket of type T representing the
  //    socket to monitor.
  //
  template <typename T>
  void add(const T &socket) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &new_event = events_[socket.get_fd()];
    new_event.unwatch_ = false;
    new_event.on_receive_.running = false;
    new_event.on_receive_.callback = nullptr;
    new_event.on_send_.running = false;
    new_event.on_send_.callback = nullptr;
    socketpair_write();
  }

  // Set a read event to monitor on the given socket.
  //
  // @params:
  //    - [s] a reference on a const socket of type T representing the socket
  //    concerned by a read operation.
  //    - [c] a reference on a const function object representing the callback
  //    to execute when a read operation has been performed for the specified
  //    socket.
  //
  template <typename T>
  void wait_for_read(const T &s, const std::function<void(void)> &c) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &specific_event = events_[s.get_fd()];
    specific_event.unwatch_ = false;
    specific_event.on_receive_.callback = c;
    socketpair_write();
  }

  // Set a write event to monitor on the given socket.
  //
  // @params:
  //    - [s] a reference on a const socket of type T representing the socket
  //    concerned by a write operation.
  //    - [c] a reference on a const function object representing the callback
  //    to execute when a write operation has been performed for the specified
  //    socket.
  //
  template <typename T>
  void wait_for_write(const T &s, const std::function<void(void)> &c) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &specific_event = events_[s.get_fd()];
    specific_event.unwatch_ = false;
    specific_event.on_send_.callback = c;
    socketpair_write();
  }

  // Stop monitoring the given socket.
  //
  //  @param:
  //    - [socket] a reference on a const socket of type T representing the
  //    socket to remove from the polling model.
  //
  template <typename T>
  void remove(const T &socket) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    if (events_.find(socket.get_fd()) == events_.end()) return;

    auto &target = events_[socket.get_fd()];

    if (target.on_receive_.running || target.on_send_.running) {
      target.unwatch_ = true;
      return;
    } else {
      auto iterator = events_.find(socket.get_fd());
      events_.erase(iterator);
    }
    socketpair_write();
  }

 private:
  // Force poll to wake up by writting a byte on the file descriptor which
  // refers to the other connected socketpair.
  void socketpair_write(void) { (void)::write(socketpair_[1], "H", 1); }

  // Read data out of the other connected socketpair.
  void socketpair_read(void) {
    char buffer[tools::BUFFER_SIZE];
    (void)::read(socketpair_[0], buffer, tools::BUFFER_SIZE);
  }

  // Synchronize the differents events monitored by the poller. Fill the vector
  // of pollfd structures with all information for the file descriptors waiting
  // for an I/O operation.
  void synchronize_events(void) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    poll_structs_.clear();
    for (auto &event : events_) {
      event.second.update(event.first);
      if (event.second.has()) {
        poll_structs_.push_back(std::move(event.second.get_poll_struct()));
        event.second.reset_poll_struct();
      }
    }
    poll_structs_.push_back({socketpair_[0], POLLIN, 0});
  }

  // Handles a specific detected event for a given file descriptor by adding
  // the execution of the dedicated callback to the job queue.
  //
  // @params:
  //    - [file_descriptor] int representing the file descriptor associated to
  //    the event.
  //    - [event] object representing the type of event monitored.
  //    - [revent] short representing  the result from poll() for a registered
  //    event.
  //
  void handle_event(int file_descriptor, event &event, short revent) {
    auto fd = file_descriptor;
    bool pollin = revent & POLLIN;
    auto callback =
        pollin ? event.on_receive_.callback : event.on_send_.callback;

    if (pollin)
      event.on_receive_.running = true;
    else
      event.on_send_.running = true;

    workers_.enqueue_job([=]() {
      callback();
      std::unique_lock<std::mutex> lock(mutex_events_);
      if (events_.find(fd) == events_.end()) return;

      auto &event = events_.find(fd)->second;

      if (pollin) {
        event.on_receive_.running = false;
        if (event.unwatch_ && !event.on_send_.running)
          events_.erase(events_.find(fd));
      } else {
        event.on_send_.running = false;
        if (event.unwatch_ && !event.on_receive_.running)
          events_.erase(events_.find(fd));
      }
    });
  }

  // Processes the events detected in result of the poll operation.
  void process_detected_events(void) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    for (const auto &result : poll_structs_) {
      if (result.fd == socketpair_[0] && result.revents & POLLIN) {
        socketpair_read();
        continue;
      }

      if (events_.find(result.fd) == events_.end()) continue;

      auto &socket = events_.find(result.fd)->second;

      if (result.revents & POLLOUT && socket.on_send_.callback &&
          !socket.on_send_.running)
        handle_event(result.fd, socket, result.revents);

      if (result.revents & POLLIN && socket.on_receive_.callback &&
          !socket.on_receive_.running)
        handle_event(result.fd, socket, result.revents);
    }
  }

 private:
  // Boolean to know if the poller should stop.
  std::atomic_bool stop_;

  // thread pool to execute callbacks.
  workers workers_;

  // Main thread performing the poll.
  std::thread poll_master_;

  // Mutex to synchronize the events monitored.
  std::mutex mutex_events_;

  // Unamed pair of connected socket used to wake up the poll thread.
  int socketpair_[2];

  // A map containing:
  // @key: file descriptor
  // @value: class event representing the event monitored.
  std::unordered_map<int, event> events_;

  // An array of pollfd structures on which we will poll waiting for an I/O
  // operation.
  std::vector<struct pollfd> poll_structs_;
};

#endif  // _WIN32

// Poller singleton default instance.
static std::shared_ptr<poller> poller_g = nullptr;

// Setter poller instance.
void set_poller(const std::shared_ptr<poller> &s) { poller_g = s; }

// Getter poller instance.
const std::shared_ptr<poller> &get_poller(void) {
  if (!poller_g) poller_g = std::make_shared<poller>();
  return poller_g;
}

namespace network {

namespace tcp {

// TCP client.
class client final : _default_signatures_ {
 public:
  client(void) : connected_(false), poller_(get_poller()) {}

  client(socket &&socket)
      : socket_(std::move(socket)), connected_(true), poller_(get_poller()) {
    poller_->add<tcp::socket>(socket_);
  }

  ~client(void) { disconnect(); }

 public:
  // Callback executed when a send operation has been performed.
  typedef std::function<void(bool, std::size_t)> async_send_callback;
  // Callback executed when a receive operation has been performed.
  typedef std::function<void(bool, std::vector<char>)> async_receive_callback;

 public:
  // Returns true or false whether the client is connected.
  bool is_connected(void) const { return connected_ == true; }

  // Returns the client's socket.
  const socket &get_socket(void) const { return socket_; }

 private:
  // Send callback.
  void on_send(void) {
    std::unique_lock<std::mutex> lock(send_requests_mutex_);

    if (send_requests_.empty()) return;

    bool success = false;
    std::size_t result = 0;
    auto request = send_requests_.front();
    auto buffer = request.first;
    auto callback = request.second;

    try {
      result = socket_.send(buffer, buffer.size());
      success = true;
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      success = false;
    }

    send_requests_.pop();

    if (!success) disconnect();

    if (callback) callback(success, result);
  }

  // Receive callback.
  void on_receive(void) {
    std::unique_lock<std::mutex> lock(receive_requests_mutex_);

    if (receive_requests_.empty()) return;

    bool success = false;
    std::vector<char> result;
    auto request = receive_requests_.front();
    auto size_to_read = request.first;
    auto callback = request.second;

    try {
      result = socket_.receive(size_to_read);
      success = true;
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      success = false;
    }

    receive_requests_.pop();

    if (!success) disconnect();

    if (callback) callback(success, result);
  }

 public:
  // Connect the client to the given host/port.
  //
  // @params:
  //  - [host] a reference on a const string containing the address of the
  //  remote server that we want to connect.
  //  - [port] unsigned int representing the port of the remote server that we
  //  want to connect.
  //
  void connect(const std::string &host, unsigned int port) {
    if (is_connected()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "error: The client is already connected.");
    }

    socket_.connect(host, port);
    poller_->add<tcp::socket>(socket_);
    connected_ = true;
  }

  // Async send operation.
  //
  //  @params:
  //    - [str] a reference on a const string containing the data to send.
  //    - [callback] a reference on a const async_send_callback containing the
  //    callback to execute when data has been sent.
  //

  void async_send(const std::string &str, const async_send_callback &callback) {
    async_send(std::vector<char>(str.begin(), str.end()), callback);
  }

  // Async send operation.
  //
  //  @params:
  //    - [data] a reference on a const vector of char containing the data to
  //    send.
  //    - [callback] a reference on a const async_send_callback containing the
  //    callback to execute when data has been sent.
  //
  void async_send(const std::vector<char> &data,
                  const async_send_callback &callback) {
    if (!is_connected()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: The client must be connected before "
                   "being able to perform an asynchronous send of data.");
    }

    std::unique_lock<std::mutex> lock(send_requests_mutex_);

    if (callback) {
      send_requests_.push(std::make_pair(data, callback));
      poller_->wait_for_write<tcp::socket>(socket_,
                                           std::bind(&client::on_send, this));
    } else {
      std::cout << "WARNING: You must provide a callback\n";
    }
  }

  // Async receive operation.
  //
  // @params:
  //    - [size] std::size_t representing the number of bytes that we want to
  //    receive.
  //    - [callback] a reference on a const async_receive_callback containing
  //    the callback to execute when data has been received.
  //
  void async_receive(std::size_t size, const async_receive_callback &callback) {
    if (!is_connected()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: The client must be connected before "
                   "being able to perform an asynchronous receive of data.");
    }

    std::unique_lock<std::mutex> lock(receive_requests_mutex_);

    if (callback) {
      receive_requests_.push(std::make_pair(size, callback));
      poller_->wait_for_read<tcp::socket>(socket_,
                                          std::bind(&client::on_receive, this));
    } else {
      std::cout << "WARNING: You must provide a callback\n";
    }
  }

  // Disconnect the client.
  void disconnect(void) {
    if (!is_connected()) return;

    connected_ = false;
    poller_->remove<tcp::socket>(socket_);

    try {
      socket_.close();
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  }

 private:
  // Client's socket.
  socket socket_;

  // Boolean to know if the client is already connected.
  std::atomic_bool connected_;

  // A smart pointer on the polling instance.
  std::shared_ptr<poller> poller_;

  // Mutex to synchronize the queue of send requests.
  std::mutex send_requests_mutex_;

  // Mutex to synchronize the queue of receive requests.
  std::mutex receive_requests_mutex_;

  // A queue containing the send requests.
  std::queue<std::pair<std::vector<char>, async_send_callback>> send_requests_;

  // A queue containing the receive requests.
  std::queue<std::pair<std::size_t, async_receive_callback>> receive_requests_;
};

// TCP server.
class server final : _default_signatures_ {
 public:
  server(void) : running_(false), poller_(get_poller()) {}

  ~server(void) { stop(); }

 public:
  // Returns true or false whether the server is running.
  bool is_running(void) const { return running_; }

  // Returns the server's socket.
  const socket &get_socket(void) const { return socket_; }

 private:
  // Function executed when a client is trying to connect to this server.
  void on_accept(void) {
    std::unique_lock<std::mutex> lock(mutex_);

    try {
      auto new_client = std::make_shared<client>(socket_.accept());
      if (callback_) callback_(new_client);
      clients_.insert(new_client);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      stop();
    }
  }

 public:
  // This function provides a callback that the server stores and will execute
  // on a new connection.
  //
  // @param:
  //    - [callback] a reference on a const function object containing the
  //    callback to execute when a client is trying to connect to this server.
  //
  void on_connection(
      const std::function<void(const std::shared_ptr<client> &)> &callback) {
    callback_ = callback;
  }

  // Runs the server on the given host and port.
  //
  // @params:
  //    - [host] a reference on a const string containing the address to which
  //    we wan to run the server.
  //    - [port] unsigned int representing the port on which we want to run the
  //    server..
  //
  void run(const std::string &host, unsigned int port) {
    if (is_running()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   "Invalid operation: The server is already running.");
    }

    if (!callback_) {
      HERMES_THROW(error::ARGS, __PRETTY_FUNCTION__, __LINE__,
                   "error: You MUST provide a callback to the server in case "
                   "of connection using the 'on_connection' member function "
                   "before running the server.");
    }

    socket_.bind(host, port);
    socket_.listen(tools::BACKLOG);
    poller_->add<tcp::socket>(socket_);
    poller_->wait_for_read<tcp::socket>(socket_,
                                        std::bind(&server::on_accept, this));
    running_ = true;
  }

  // Stop the server.
  void stop(void) {
    if (!is_running()) return;

    std::unique_lock<std::mutex> lock(mutex_);
    running_ = false;
    poller_->remove<tcp::socket>(socket_);

    try {
      socket_.close();
      for (auto &client : clients_) client->disconnect();
      clients_.clear();
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  }

 private:
  // Server's socket.
  socket socket_;

  // A mutex to synchronize the set of clients.
  std::mutex mutex_;

  // Boolean to know if the server is running.
  std::atomic_bool running_;

  // A smart pointer on the polling instance.
  std::shared_ptr<poller> poller_;

  // A set of clients connected to this server.
  std::unordered_set<std::shared_ptr<client>> clients_;

  // A callback executed when a new client is accepted.
  std::function<void(const std::shared_ptr<client> &)> callback_;
};

}  // namespace tcp

namespace udp {

// UDP client.
class client final : _default_signatures_ {
 public:
  client(void) : poller_(get_poller()), broadcast_mode_(false) {}

  ~client(void) { stop(); }

 public:
  // Callback executed when a send operation has been performed.
  typedef std::function<void(int)> async_send_callback;

 public:
  // Returns true if the broadcast mode is enabled, false otherwise.
  bool broadcast_mode_enabled(void) const { return broadcast_mode_; }

  // Returns the client's socket.
  const socket &get_socket(void) const { return socket_; }

 private:
  // Send callback.
  void on_send(void) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (send_requests_.empty()) return;

    int result = -1;
    auto request = send_requests_.front();
    auto buffer = request.first;
    auto callback = request.second;

    try {
      if (broadcast_mode_)
        result = socket_.broadcast(buffer, buffer.size());
      else
        result = socket_.sendto(buffer, buffer.size());
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    send_requests_.pop();

    if (callback) callback(result);
  }

 public:
  // Initialize the client.
  //
  //@params:
  //    - [host] a reference on a const string containing the host address..
  //    - [port] unsigned int representing the host port.
  //    - [broadcast_mode] boolean to enable or disable the broadcast mode.
  //
  void init(const std::string host, unsigned int port, bool broadcast_mode) {
    socket_.init_datagram_socket(host, port, broadcast_mode);
    broadcast_mode_ = broadcast_mode;
    poller_->add<udp::socket>(socket_);
  }

  // Asynchronous send of data.
  //
  // @params:
  //    - [str] a reference on a const string containing the data to sent.
  //    - [callback] a reference on a const async_send_callback representing the
  //    callback to execute when the data has been sent.
  //
  void async_send(const std::string &str, const async_send_callback &callback) {
    async_send(std::vector<char>(str.begin(), str.end()), callback);
  }

  // Asynchronous send of data.
  //
  // @params:
  //    - [data] a reference on a const vector of char containing the data to
  //    sent.
  //    - [callback] a reference on a const async_send_callback representing the
  //    callback to execute when the data has been sent.
  //
  void async_send(const std::vector<char> &data,
                  const async_send_callback &callback) {
    if (broadcast_mode_enabled()) {
      std::cout << "INFO: broadcast mode enabled. Use 'async_broadcast'.\n";
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    if (callback) {
      send_requests_.push(std::make_pair(data, callback));
      poller_->wait_for_write<udp::socket>(socket_,
                                           std::bind(&client::on_send, this));
    } else {
      std::cout << "WARNING: You must provide a callback\n";
    }
  }

  // Asynchronous broadcast of data.
  //
  // @params:
  //    - [str] a reference on a const string containing the data to broadcast.
  //    - [callback] a reference on a const async_send_callback representing the
  //    callback to execute when the data has been broadcasted.
  //
  void async_broadcast(const std::string &str,
                       const async_send_callback &callback) {
    async_broadcast(std::vector<char>(str.begin(), str.end()), callback);
  }

  // Asynchronous broadcast of data.
  //
  // @params:
  //    - [data] a reference on a const vector of char containing the data to
  //    broadcast.
  //    - [callback] a reference on a const async_send_callback representing the
  //    callback to execute when the data has been broadcasted.
  //
  void async_broadcast(const std::vector<char> &data,
                       const async_send_callback &callback) {
    if (!broadcast_mode_enabled()) {
      std::cout << "INFO: broadcast mode disabled. Use 'async_send'.\n";
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    if (callback) {
      send_requests_.push(std::make_pair(data, callback));
      poller_->wait_for_write<udp::socket>(socket_,
                                           std::bind(&client::on_send, this));
    } else {
      std::cout << "WARNING: You must provide a callback\n";
    }
  }

  // Stop the client.
  void stop(void) {
    broadcast_mode_ = false;
    poller_->remove<udp::socket>(socket_);
    try {
      socket_.close();
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  }

 private:
  // Client's socket.
  socket socket_;

  // Mutex to synchronize the queue of send requests.
  std::mutex mutex_;

  // A smart pointer on the polling instance.
  std::shared_ptr<poller> poller_;

  // Boolean to know if the broadcast mode is enabled.
  std::atomic_bool broadcast_mode_;

  // A queue containing the send requests.
  std::queue<std::pair<std::vector<char>, async_send_callback>> send_requests_;
};

// UDP server.
class server final : _default_signatures_ {
 public:
  server(void) : poller_(get_poller()), callback_(nullptr) {}

  ~server(void) { stop(); }

 public:
  // Callback executed when a receive operation has been performed.
  typedef std::function<void(std::vector<char>, int)> async_receive_callback;

 public:
  // Returns true if the socket is bound, false otherwise.
  bool is_running(void) const { return socket_.has_a_name_assigned(); }

  // Returns the server's socket.
  const socket &get_socket(void) const { return socket_; }

 private:
  // Receive callback.
  void on_receive(void) {
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    int result = -1;
    std::vector<char> buffer;

    buffer.reserve(tools::BUFFER_SIZE);

    try {
      result = socket_.recvfrom(buffer);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }

    if (callback_) {
      callback_(std::move(buffer), result);
      poller_->wait_for_read<udp::socket>(socket_,
                                          std::bind(&server::on_receive, this));
    }
  }

 public:
  // Bind the server on the given host/port.
  //
  // @params:
  //    - [host] a reference on a const string containing the address to assign
  //    to the socket.
  //    - [port] unsigned int representing the port to assign to the socket.
  //
  void bind(const std::string &host, unsigned int port) {
    if (socket_.has_a_name_assigned()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   " Invalid operation: You need to bind the server before.");
    }

    socket_.bind(host, port);
    poller_->add<udp::socket>(socket_);
    poller_->wait_for_read<udp::socket>(socket_,
                                        std::bind(&server::on_receive, this));
  }

  // Asynchronous receive of data.
  //
  // @param:
  //    - [callback] a reference on a const async_receive_callback representing
  //    the callback to execute when re te receive operation has been performed.
  //
  void async_recvfrom(const async_receive_callback &callback) {
    if (!socket_.has_a_name_assigned()) {
      HERMES_THROW(error::LOGIC, __PRETTY_FUNCTION__, __LINE__,
                   " Invalid operation: You need to bind the server before.");
    }

    if (callback) {
      callback_ = callback;
    } else {
      std::cout << "WARNING: You must provide a callback\n";
    }
  }

  // Stop the server.
  void stop(void) {
    poller_->remove<udp::socket>(socket_);
    try {
      socket_.close();
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  }

 private:
  // Server's socket.
  socket socket_;

  // A smart pointer on the polling instance.
  std::shared_ptr<poller> poller_;

  // The callback executed when data has been received.
  std::function<void(std::vector<char>, int)> callback_;
};

}  // namespace udp
}  // namespace network
}  // namespace hermes
