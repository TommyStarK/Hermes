#pragma once

#define __windows__ _WIN32 || _WIN64

#ifdef __linux__
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#elif __windows__
#include <WinSock2.h>
#endif

#include <atomic>
#include <condition_variable>
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

// Default size for the maximum length to which the queue for pending
// connections may grow.
static unsigned int const BACKLOG = 100;
// Default size used for buffers.
static unsigned int const BUFFER_SIZE = 8096;
// Default number of concurrent threads supported by the system.
// Threads are used as 'workers'.
static unsigned int const THREADS_NBR = std::thread::hardware_concurrency();

// Format the error to provide an understandable output.
std::string format_error(const std::string &msg) {
  return std::string("[hermes ") + std::string(__FILE__) + std::string(":") +
         std::to_string(__LINE__) + std::string("]\n") + msg;
}

// Various defines to report common errors.
#define __LOGIC_ERROR__(error) throw std::logic_error(format_error(error));
#define __RUNTIME_ERROR__(error) throw std::runtime_error(format_error(error));
#define __INVALID_ARG__(error) throw std::invalid_argument(format_error(error));
#define __DISPLAY_ERROR__(error) std::cerr << format_error(error) << std::endl;

// A thread pool waiting for jobs for concurrent execution.
// Jobs are enqueued in a synchronized queue. Each worker is waiting for job.
class workers {
 public:
  explicit workers(unsigned int workers_nbr = THREADS_NBR) : stop_(false) {
    // Check the number of concurrent threads supported by the system.
    if (workers_nbr > std::thread::hardware_concurrency())
      __LOGIC_ERROR__(
          "tools::workers::constructor: Number of workers is greater than the"
          "number of concurrent threads supported by the system\n.");

    // We start the workers.
    for (unsigned int i = 0; i < workers_nbr; ++i)
      workers_.push_back(std::thread([this]() {
        // Worker routine:
        // Each worker is waiting for a new job. The first worker who can
        // process a job, removes it from the queue and executes it.

        // We loop waiting for a new job.
        while (!stop_) {
          auto job = retrieve_job();
          if (job) job();
        }

      }));
  }

  workers(const workers &) = delete;

  workers &operator=(const workers &) = delete;

  ~workers() { stop(); }

 public:
  // Stop the thread pool.
  void stop(void) {
    if (stop_) return;

    stop_ = true;
    // We notify all threads that workers should stop working in order to
    // join every worker.
    condition_.notify_all();

    for (auto &worker : workers_) worker.join();
    workers_.clear();
  }

  // Allows the user to enqueue a new job which must be processed.
  // It will notify every threads that a job has been enqueued.
  void enqueue_job(const std::function<void(void)> &new_job) {
    if (!new_job)
      __LOGIC_ERROR__(
          "tools::workers::enqueue_job: Passing nullptr instead of const "
          "std::function<void(void)> &.");

    std::unique_lock<std::mutex> lock(mutex_job_queue_);
    job_queue_.push(new_job);
    condition_.notify_all();
  }

  // Returns true or false whether workers are working.
  bool are_working(void) const { return !stop_; }

 private:
  // Check the job queue to know if there is a job waiting. If that is the case
  // it returns the job and removes it from the queue.
  std::function<void(void)> retrieve_job() {
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

  // Thread pool
  std::vector<std::thread> workers_;

  // Contains pending job
  std::queue<std::function<void(void)>> job_queue_;
};

}  // namespace tools

namespace network {

using namespace tools;

namespace tcp {

#ifdef __linux__

class socket {
 public:
  // Basic constructor.
  socket(void) : fd_(-1), host_(""), port_(0), is_socket_bound_(false) {}

  // Create a socket from an existing file descriptor.
  socket(int fd, const std::string &host, unsigned int port)
      : fd_(fd), host_(host), port_(port), is_socket_bound_(false) {}

  // A move constructor has been implemented allowing to construct a socket
  // from a rvalue of an existing socket.
  socket(socket &&socket)
      : fd_(std::move(socket.get_fd())),
        host_(socket.get_host()),
        port_(socket.get_port()),
        v_addrinfo_(std::move(socket.get_struct_addrinfo())),
        is_socket_bound_(false) {
    socket.fd_ = -1;
  }

  socket(const socket &) = delete;

  socket &operator=(const socket &) = delete;

  bool operator==(const socket &s) const { return fd_ == s.get_fd(); }

  ~socket(void) = default;

 public:
  //
  // Server operations.
  //

  // Assign a name to the socket.
  void bind(const std::string &host, unsigned int port) {
    int yes = 1;
    host_ = host;
    port_ = port;
    get_addr_info();
    create_socket();

    if (is_socket_bound_)
      __LOGIC_ERROR__("tcp::socket::bind: Socket is  already bound to" + host_ +
                      ":" + std::to_string(port_));

    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
      __RUNTIME_ERROR__("tcp::socket::bind: setsockopt() failed.");

    if (::bind(fd_, v_addrinfo_.ai_addr, v_addrinfo_.ai_addrlen) == -1)
      __RUNTIME_ERROR__("tcp::socket::bind: bind() failed.");
    is_socket_bound_ = true;
  }

  // Mark the socket as passive socket.
  void listen(unsigned int backlog = tools::BACKLOG) {
    if (not is_socket_bound_)
      __LOGIC_ERROR__(
          "tcp::socket::listen: Socket must be bound before listening for "
          "incoming connections.");

    if (backlog > SOMAXCONN)
      __DISPLAY_ERROR__(
          "tcp::socket::listen: Param backlog greater than "
          "SOMAXCONN.\nPlease "
          "refer to the value in /proc/sys/net/core/somaxconn. Param backlog "
          "will be truncated.");

    if (::listen(fd_, backlog) == -1)
      __RUNTIME_ERROR__("tcp::socket::listen: listen() failed.");
  }

  // Accept a new connection.
  tcp::socket accept(void) {
    socklen_t size;
    char host[NI_MAXHOST];
    char port[NI_MAXSERV];
    struct sockaddr_storage client;

    size = sizeof(client);
    int new_fd = ::accept(fd_, (struct sockaddr *)&client, &size);

    if (new_fd == -1)
      __RUNTIME_ERROR__("tcp::socket::accpet: accept() failed.");

    int res = getnameinfo((struct sockaddr *)&client, size, host, sizeof(host),
                          port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

    if (res != 0)
      __RUNTIME_ERROR__("tcp::socket::accept: getnameinfo() failed.");

    return {new_fd, std::string(host), (unsigned int)std::stoi(port)};
  }

  //
  // Client operations.
  //

  // Connect to a remote host.
  void connect(const std::string &host, unsigned int port) {
    if (is_socket_bound_)
      __LOGIC_ERROR__(
          "tcp::socket::connect: Trying to connect a socket bound on port: " +
          std::to_string(port_) +
          ". Invalid operation for a socket planned for a server application.");

    host_ = host;
    port_ = port;
    get_addr_info();
    create_socket();

    if (::connect(fd_, v_addrinfo_.ai_addr, v_addrinfo_.ai_addrlen) == -1)
      __RUNTIME_ERROR__("tcp::socket::connect: connect() failed.");
  }

  // Send data.
  std::size_t send(const std::string &message) {
    return send(std::vector<char>(message.begin(), message.end()),
                message.size());
  }

  // Send data.
  std::size_t send(const std::vector<char> &message, std::size_t message_len) {
    if (fd_ == -1)
      __LOGIC_ERROR__(
          "tcp::socket::send: Invalid operation. Trying to send data on a non "
          "connected socket.");

    int res = ::send(fd_, message.data(), message_len, 0);

    if (res == -1) __RUNTIME_ERROR__("tcp::socket::send: send() failed.");

    return res;
  }

  // Receive data.
  std::vector<char> receive(std::size_t size_to_read = tools::BUFFER_SIZE) {
    if (fd_ == -1)
      __LOGIC_ERROR__(
          "tcp::socket::send: Invalid operation. Trying to receive data on a "
          "non connected socket.");

    std::vector<char> buffer(size_to_read, 0);

    int bytes_read =
        ::recv(fd_, const_cast<char *>(buffer.data()), size_to_read, 0);

    switch (bytes_read) {
      case -1:
        __RUNTIME_ERROR__("tcp::socket::receive: recv() failed.");
        break;
      case 0:
        std::cout << "Connection closed.\n";
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

  // Close the file descriptor associated to the socket.
  void close(void) {
    if (fd_ != -1) {
      if (::close(fd_) == -1)
        __RUNTIME_ERROR__("tcp::socket::close: close() failed.");
    }
    fd_ = -1;
  }

 public:
  // Returns the file descriptor associated to the socket.
  int get_fd(void) const { return fd_; }

  // Returns the socket address.
  const std::string &get_host(void) const { return host_; }

  // Returns the socket port.
  unsigned int get_port(void) const { return port_; }

  // Returns true or false whether the socket is bound.
  bool is_socket_bound(void) const { return is_socket_bound_; }

  // Returns a reference on a structure containing a network address used by the
  // socket.
  struct addrinfo &get_struct_addrinfo(void) {
    return v_addrinfo_;
  }

 private:
  // With given Internet host and service, get_addr_info() tries to retrieve
  // a list of structures containing each, a network address that matches
  // host and port.
  void get_addr_info(void) {
    int status;
    struct addrinfo hints;
    struct addrinfo *infos;

    ::memset(&hints, 0, sizeof(hints));
    ::memset(&addrinfo_, 0, sizeof(addrinfo_));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = ::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(),
                                &hints, &infos)) != 0)
      __RUNTIME_ERROR__("tcp::socket::get_addr_info: getaddrinfo() failed.");

    if (infos) ::memmove(&addrinfo_, infos, sizeof(*infos));
  }

  // Create an endpoint for communication.
  void create_socket(void) {
    if (fd_ != -1) return;

    for (auto p = &addrinfo_; p != NULL; p = p->ai_next) {
      if ((fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        continue;

      ::memset(&v_addrinfo_, 0, sizeof(*p));
      ::memcpy(&v_addrinfo_, p, sizeof(*p));
      break;
    }

    if (fd_ == -1)
      __RUNTIME_ERROR__("tcp::socket::create_socket: socket failed().");
  }

 private:
  // file descriptor associated to the socket.
  int fd_;

  // Socket address.
  std::string host_;

  // Socket port.
  int port_;

  // List of structures containing each, a network address.
  struct addrinfo addrinfo_;

  // Network address used by the socket.
  struct addrinfo v_addrinfo_;

  // Boolean to know if the socket is bound.
  bool is_socket_bound_;
};

#elif __windows__

class socket {
  socket() {}
  ~socket() = default;
  int fd_;
  int get_fd() const { return fd_; }
};

#endif

}  // namespace tcp

namespace udp {

#ifdef __linux__

class socket {
 public:
  socket() {}
  ~socket() = default;
  int fd_;
  int get_fd() const { return fd_; }
};

#elif __windows__

class socket {
 public:
  socket() {}
  ~socket() = default;
  int fd_;
  int get_fd() const { return fd_; }
};

#endif

}  // namespace udp
}  // namespace network

// Represents an expected event for a file descriptor.
class event {
 public:
  event(void)
      : unwatch_(false),
        is_executing_send_callback_(false),
        is_executing_receive_callback_(false),
        send_callback_(nullptr),
        receive_callback_(nullptr) {}

  ~event(void) = default;

 public:
  // Returns true if a callback is already running, false otherwise.
  bool is_there_a_callback_already_running(void) {
    return !is_executing_send_callback_ && !is_executing_receive_callback_
               ? false
               : true;
  }

 public:
  // Events watcher should not anymore watch this file descriptor.
  std::atomic_bool unwatch_;

  // Boolean to know if we are executing a callback (send operation).
  std::atomic_bool is_executing_send_callback_;

  // Boolean to know if we are executing a callback (receive operation).
  std::atomic_bool is_executing_receive_callback_;

  // The callback to execute if the file descriptor is ready for sending data.
  std::function<void(void)> send_callback_;

  // The callback to execute if the file descriptor is ready for receiving data.
  std::function<void(void)> receive_callback_;
};

// An events watcher uses poll() API to determine if a file descriptor is ready
// for a specific operation (send or receive data).
class events_watcher {
 public:
  events_watcher(void) : stop_(false) {}

  events_watcher(const events_watcher &) = delete;

  events_watcher &operator=(const events_watcher &) = delete;

  ~events_watcher(void) {
    stop_ = true;
    workers_.stop();
  }

 public:
  // Returns true or false whether an event is registered for a specific fd.
  template <typename T>
  bool is_an_event_registered(const T &socket) {
    return events_registered_.find(socket.get_fd()) == events_registered_.end()
               ? false
               : true;
  }

  // Start watching on a file descriptor.
  template <typename T>
  void watch(const T &socket) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &new_event = events_registered_[socket.get_fd()];
    new_event.unwatch_ = false;
    new_event.is_executing_send_callback_ = false;
    new_event.is_executing_receive_callback_ = false;
    new_event.send_callback_ = nullptr;
    new_event.receive_callback_ = nullptr;
  }

  // Set a callback to a specific file descriptor for a receive operation. The
  // callback will be executed if the file descriptor is available for receiving
  // data.
  template <typename T>
  void on_receive_callback(const T &socket,
                           const std::function<void(void)> &callback) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &specific_event = events_registered_[socket.get_fd()];
    specific_event.unwatch_ = false;
    specific_event.receive_callback_ = callback;
  }

  // Set a callback to a specific file descriptor for a send operation. The
  // callback will be executed if the file descriptor is available for sending
  // data.
  template <typename T>
  void on_send_callback(const T &socket,
                        const std::function<void(void)> &callback) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &specific_event = events_registered_[socket.get_fd()];
    specific_event.unwatch_ = false;
    specific_event.send_callback_ = callback;
  }

  // Stop watching on a file descriptor.
  template <typename T>
  void unwatch(const T &socket) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    if (events_registered_.find(socket.get_fd()) == events_registered_.end())
      return;

    auto &socket_to_unwatch = events_registered_[socket.get_fd()];

    if (socket_to_unwatch.is_there_a_callback_already_running()) {
      socket_to_unwatch.unwatch_ = true;
      return;
    } else {
      auto iterator = events_registered_.find(socket.get_fd());
      events_registered_.erase(iterator);
    }
  }

 private:
  // Boolean to know if the events watcher should stop.
  std::atomic_bool stop_;

  // Thread pool to execute callbacks.
  tools::workers workers_;

  // Mutex to synchronize the events registered.
  std::mutex mutex_events_;

  // A map containing:
  // @key: file descriptor
  // @value: class event
  std::unordered_map<int, event> events_registered_;
};

// Events watcher singleton.
static std::shared_ptr<events_watcher> events_watcher_singleton = nullptr;

// Events watcher singleton setter.
void set_events_watcher(const std::shared_ptr<events_watcher> &watcher) {
  events_watcher_singleton = watcher;
}

// Events watcher singleton getter.
const std::shared_ptr<events_watcher> &get_events_watcher(void) {
  if (!events_watcher_singleton)
    events_watcher_singleton = std::make_shared<events_watcher>();
  return events_watcher_singleton;
}

namespace network {

namespace tcp {

// TCP client.
class client {
 public:
  client(void) : connected_(false), events_watcher_(get_events_watcher()) {}

  client(socket &&socket)
      : socket_(std::move(socket)),
        connected_(true),
        events_watcher_(get_events_watcher()) {
    events_watcher_->watch<tcp::socket>(socket_);
  }

  client(const client &) = delete;

  client &operator=(const client &) = delete;

  ~client(void) { disconnect(); }

 public:
  typedef std::function<void(bool, std::size_t)> async_send_callback;
  typedef std::function<void(bool, std::vector<char>)> async_receive_callback;

 public:
  // Returns true or false whether the client is connected.
  bool is_connected(void) const { return connected_; }

  // Returns the client's socket.
  const socket &get_socket(void) const { return socket_; }

  // Connect the client to the given host/port.
  void connect(const std::string &host, unsigned int port) {
    if (connected_)
      __LOGIC_ERROR__("tcp::client::connect: The client is already connected.");
    socket_.connect(host, port);
    events_watcher_->watch<tcp::socket>(socket_);
    connected_ = true;
  }

  // Disconnect the client.
  void disconnect(void) {
    if (!connected_) return;

    connected_ = false;
    events_watcher_->unwatch<tcp::socket>(socket_);
    socket_.close();
  }

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
      result = socket_.send(std::string(buffer.begin(), buffer.end()));
      success = true;
    } catch (const std::exception &e) {
      __DISPLAY_ERROR__(e.what());
      success = false;
    }

    send_requests_.pop();

    if (send_requests_.empty())
      events_watcher_->on_send_callback<tcp::socket>(socket_, nullptr);

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
      __DISPLAY_ERROR__(e.what());
      success = false;
    }

    receive_requests_.pop();

    if (receive_requests_.empty())
      events_watcher_->on_receive_callback<tcp::socket>(socket_, nullptr);

    if (!success) disconnect();

    if (callback) callback(success, result);
  }

 public:
  // Async send operation.
  void async_send(const std::string &str, const async_send_callback &callback) {
    async_send(std::vector<char>(str.begin(), str.end()), callback);
  }

  // Async send operation.
  void async_send(std::vector<char> data, const async_send_callback &callback) {
    if (!connected_)
      __LOGIC_ERROR__(
          "tcp::client::async_send: You must connect the client before trying "
          "to send data.");

    std::unique_lock<std::mutex> lock(send_requests_mutex_);

    if (callback) {
      send_requests_.push(std::make_pair(data, callback));
      events_watcher_->on_send_callback<tcp::socket>(
          socket_, std::bind(&client::on_send, this));
    }
  }

  // Async receive operation.
  void async_receive(std::size_t size, const async_receive_callback &callback) {
    if (!connected_)
      __LOGIC_ERROR__(
          "tcp::client::async_receive: You must connect the client before "
          "trying to receive data.");

    std::unique_lock<std::mutex> lock(receive_requests_mutex_);

    if (callback) {
      receive_requests_.push(std::make_pair(size, callback));
      events_watcher_->on_receive_callback<tcp::socket>(
          socket_, std::bind(&client::on_receive, this));
    }
  }

 private:
  // Client's socket.
  socket socket_;

  // Mutex to synchronize the queue of send requests.
  std::mutex send_requests_mutex_;

  // Mutex to synchronize the queue of receive requests.
  std::mutex receive_requests_mutex_;

  // A queue containing send requests.
  std::queue<std::pair<std::vector<char>, async_send_callback>> send_requests_;

  // A queue containing receive requests.
  std::queue<std::pair<std::size_t, async_receive_callback>> receive_requests_;

  // Boolean to know if the client is already connected.
  std::atomic_bool connected_;

  // A smart pointer on the events watcher singleton.
  std::shared_ptr<events_watcher> events_watcher_;
};

// TCP server.
class server {
 public:
  server(void) : running_(false), events_watcher_(get_events_watcher()) {}

  server(const server &) = delete;

  server &operator=(const server &) = delete;

  ~server(void) { stop(); }

 public:
  // Returns true or false whether the server is running.
  bool is_running(void) const { return running_; }

  // This function provides a callback that the server executes on a new
  // connection.
  void on_connection(
      const std::function<void(const std::shared_ptr<client> &)> &callback) {
    callback_ = callback;
  }

  // Start the server.
  void run(const std::string &host, unsigned int port) {
    if (running_)
      __LOGIC_ERROR__("tcp::server::run: Server is already running.");

    if (!callback_)
      __LOGIC_ERROR__(
          "tcp::server::run: You must provide a callback for a new "
          "connection.\n Use method on_connection(const std::function<const "
          "std::shared_ptr<client> &> &callback) before running the server.");

    socket_.bind(host, port);
    socket_.listen();
    events_watcher_->watch<tcp::socket>(socket_);
    events_watcher_->on_receive_callback<tcp::socket>(
        socket_, std::bind(&server::on_accept, this));
    running_ = true;
  }

  // Stop the server.
  void stop(void) {
    if (!running_) return;

    std::unique_lock<std::mutex> lock(mutex_);
    running_ = false;
    events_watcher_->unwatch<tcp::socket>(socket_);
    socket_.close();
    for (auto &client : clients_) client->disconnect();
    clients_.clear();
  }

 private:
  // This function is triggered by the events watcher and a client is trying to
  // connect to our server.
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

 private:
  // Server's socket.
  socket socket_;

  // A mutex to synchronize the set of clients.
  std::mutex mutex_;

  // Boolean to know if the server is running.
  std::atomic_bool running_;

  // A smart pointer on the events watcher.
  std::shared_ptr<events_watcher> events_watcher_;

  // A set of clients connected to our server.
  std::unordered_set<std::shared_ptr<client>> clients_;

  // A callback executed when a new client is accepted.
  std::function<void(const std::shared_ptr<client> &)> callback_;
};

}  // namespace tcp

namespace udp {

//
class client {
 public:
  client(void) : events_watcher_(get_events_watcher()) {}
  ~client() {}

 private:
  //
  socket socket_;

  //
  std::shared_ptr<events_watcher> events_watcher_;
};

//
class server {
 public:
  server(void) : events_watcher_(get_events_watcher()) {}
  ~server(void) {}

 private:
  //
  socket socket_;

  //
  std::shared_ptr<events_watcher> events_watcher_;
};

}  // namespace udp
}  // namespace network
}  // namespace hermes
