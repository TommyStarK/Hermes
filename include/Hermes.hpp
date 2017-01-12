#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

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
// Feel free to modify any value of the following variables to fit to your
// needs.

// Default timeout value (milliseconds).
// Set to infinite by default in order to allow the poller to wait indefinitely
// for an event.
#ifdef _WIN32
static INT TIMEOUT = INFINITE;
#else
static int TIMEOUT = -1;
#endif  // _WIN32

// Default size for the maximum length to which the queue for pending
// connections may grow.
static unsigned int BACKLOG = 100;
// Default size used for buffers.
static unsigned int BUFFER_SIZE = 8096;
// Default number of concurrent threads supported by the system.
static unsigned int THREADS_NBR = std::thread::hardware_concurrency();

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
// Jobs are enqueued in a synchronized queue. Each worker (thread) is waiting
// for process a job.
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

  // Returns true or false whether the workers are working.
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

  // thread pool.
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
// Provides synchronous tream-oriented socket functionality.
class socket {
 public:
  socket() {}
  ~socket() = default;

 public:
  //
  SOCKET get_fd() const { return fd_; }

 private:
  //
  SOCKET fd_;
};

#else

// Unix tcp socket.
// Provides synchronous stream-oriented socket functionality.
class socket {
 public:
  // Basic constructor.
  socket(void) : fd_(-1), host_(""), port_(0), is_socket_bound_(false) {}

  // Create a socket from an existing file descriptor.
  socket(int fd, const std::string &host, unsigned int port)
      : fd_(fd), host_(host), port_(port), is_socket_bound_(false) {}

  // Move constructor.
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

  // Mark the socket as a passive socket.
  void listen(unsigned int backlog = tools::BACKLOG) {
    if (!is_socket_bound_)
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
  // host and service.
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

  // Creates an endpoint for communication.
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

#endif  // _WIN32

}  // namespace tcp

namespace udp {

#ifdef _WIN32

// Windows udp socket.
// Provides synchronous datagram-oriented socket functionality.
class socket {
 public:
  socket() {}
  ~socket() = default;

 public:
  // Returns the file descriptor associated to the socket.
  SOCKET get_fd() const { return fd_; }

 private:
  // File descriptor associated to the socket.
  SOCKET fd_;
};

#else

// Unix udp socket.
// Provides synchronous datagram-oriented socket functionality.
class socket {
 public:
  socket() {}
  ~socket() = default;

 public:
  // Returns the file descriptor associated to the socket.
  int get_fd() const { return fd_; }

 private:
  // File descriptor associated to the socket.
  int fd_;
};

#endif  // _WIN32

}  // namespace udp
}  // namespace network

#ifdef _WIN32

// Windows event model.
class event {
 public:
  event(void) {}
  ~event(void) = default;
};

// A Windows polling wrapper.
class poller {
 public:
  poller(void) {}
  ~poller(void) {}

 private:
  // A map containing:
  // @key: file descriptor (SOCKET under Windows).
  // @value: class event representing the event monitored.
  std::unordered_map<SOCKET, event> events_;
};

#else

using namespace hermes::tools;

// Unix event model.
// The event object needs to be associated to a file descriptor refering to a
// socket. It allows to store callbacks which must be processed if the socket is
// ready for a reading or a writting operation.
// Furthermore, the event model allows to know if we are currently executing a
// specific callback.
// The event model owns a pollfd structure which will be used in the polling
// model. Event objects can update their own pollfd structure according the
// callbacks defined and the potential current execution of one of these
// callbacks.
class event {
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
  void update(int fd) {
    if (unwatch_ || (!on_receive_.callback && !on_send_.callback)) return;

    pollfd_.fd = fd;

    if (on_send_.callback && !on_send_.running) pollfd_.events |= POLLOUT;

    if (on_receive_.callback && !on_receive_.running) pollfd_.events |= POLLIN;
  }

  // Returns a reference on the pollfd structure.
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

  //
  struct pollfd pollfd_;

  // receive callback.
  event_callback_info on_send_;

  // receive callback
  event_callback_info on_receive_;
};

// A Unix polling wrapper.
// Poller provides an access to polling for any given socket. The poller is
// using a thread pool as workers to execute the callback provided when the
// socket is available for the expected event.
// Poller associates the file descriptors of the monitored sockets with 'event'
// objects to store the callbacks provided. In order to offer an asynchronous
// I/O model, we are waiting using poll() for one of a set of file descriptors
// to become ready to perform an I/O operation.
// When a file descriptor is ready, a job is enqueued in the thread pool in
// order to perform the associated callback of this operation.
class poller {
 public:
  // Construct an empty polling model.
  poller(void) : stop_(false), notification_pipe_{-1, -1} {
    if (::pipe(notification_pipe_) == -1)
      __RUNTIME_ERROR__("poller::poller: Error pipe() failed.");

    poll_master_ = std::thread([this]() {
      while (!stop_) {
        synchronize_events();

        if (::poll(const_cast<struct pollfd *>(poll_structs_.data()),
                   poll_structs_.size(), TIMEOUT) > 0)
          process_detected_events();
      }
    });
  }

  poller(const poller &) = delete;

  poller &operator=(const poller &) = delete;

  // Stop the polling model.
  //
  // Workers should stop working and the poll main thread is joined.
  ~poller(void) {
    stop_ = true;
    notify_poll();
    poll_master_.join();
    workers_.stop();
    // close the read end of the notification pipe.
    ::close(notification_pipe_[0]);
    // close the write end of the notification pipe.
    ::close(notification_pipe_[1]);
  }

 public:
  // Check if we are monitoring a specific socket.
  // Returns true if the socket is currently monitored.
  template <typename T>
  bool has(const T &s) {
    return events_.find(s.get_fd()) == events_.end() ? false : true;
  }

  // Add a socket to the polling model.
  // A socket must be added to the poller before being monitored for a specific
  // event.
  template <typename T>
  void add(const T &socket) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &new_event = events_[socket.get_fd()];
    new_event.unwatch_ = false;
    new_event.on_receive_.running = false;
    new_event.on_receive_.callback = nullptr;
    new_event.on_send_.running = false;
    new_event.on_send_.callback = nullptr;
    notify_poll();
  }

  // Set a read event to monitor on the given socket.
  //
  // param:
  // - const T& s: a reference on a const socket of type T.
  // - const std::function<void(void)> &c: a reference on a const function
  // object representing the callback to perform when there will be
  // data to read on the socket.
  template <typename T>
  void wait_for_read(const T &s, const std::function<void(void)> &c) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &specific_event = events_[s.get_fd()];
    specific_event.unwatch_ = false;
    specific_event.on_receive_.callback = c;
    notify_poll();
  }

  // Set a write event to monitor on the given socket.
  //
  // param:
  // - const T& s: a reference on a const socket of type T.
  // - const std::function<void(void)> &c: a reference on a const function
  // object representing the callback to perform when writting data on the
  // socket will not block.
  template <typename T>
  void wait_for_write(const T &s, const std::function<void(void)> &c) {
    std::unique_lock<std::mutex> lock(mutex_events_);

    auto &specific_event = events_[s.get_fd()];
    specific_event.unwatch_ = false;
    specific_event.on_send_.callback = c;
    notify_poll();
  }

  // Stop monitoring the given socket.
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
    notify_poll();
  }

 private:
  // Force poll to wake up by writting on the file descriptor which refers to
  // the write end of the pipe.
  void notify_poll() { (void)::write(notification_pipe_[1], "T", 1); }

  // Clear notification pipe by reading out data of the pipe.
  void clear_notification_pipe() {
    char buffer[1024];
    (void)::read(notification_pipe_[0], buffer, 1024);
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
    poll_structs_.push_back({notification_pipe_[0], POLLIN, 0});
  }

  // Handles a specific detected event for a given file descriptor by adding
  // the execution of the dedicated callback to the job queue.
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
      if (result.fd == notification_pipe_[0] && result.revents & POLLIN) {
        clear_notification_pipe();
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
  tools::workers workers_;

  // Main thread.
  std::thread poll_master_;

  // Mutex to synchronize the events monitored.
  std::mutex mutex_events_;

  // Unidirectional data channel used to notify poll to start polling.
  int notification_pipe_[2];

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
class client {
 public:
  client(void) : connected_(false), poller_(get_poller()) {}

  client(socket &&socket)
      : socket_(std::move(socket)), connected_(true), poller_(get_poller()) {
    poller_->add<tcp::socket>(socket_);
  }

  client(const client &) = delete;

  client &operator=(const client &) = delete;

  ~client(void) { disconnect(); }

 public:
  typedef std::function<void(bool, std::size_t)> async_send_callback;
  typedef std::function<void(bool, std::vector<char>)> async_receive_callback;

 public:
  // Returns true or false whether the client is connected.
  bool is_connected(void) const { return connected_ == true; }

  // Returns the client's socket.
  const socket &get_socket(void) const { return socket_; }

  // Connect the client to the given host/port.
  void connect(const std::string &host, unsigned int port) {
    if (connected_)
      __LOGIC_ERROR__("tcp::client::connect: The client is already connected.");
    socket_.connect(host, port);
    poller_->add<tcp::socket>(socket_);
    connected_ = true;
  }

  // Disconnect the client.
  void disconnect(void) {
    if (!connected_) return;

    connected_ = false;
    poller_->remove<tcp::socket>(socket_);
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
      poller_->wait_for_write<tcp::socket>(socket_, nullptr);

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
      poller_->wait_for_read<tcp::socket>(socket_, nullptr);

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
      poller_->wait_for_write<tcp::socket>(socket_,
                                           std::bind(&client::on_send, this));
    } else {
      __DISPLAY_ERROR__(
          "tcp::client::async_send: You must provide a callback in order to "
          "perform an asynchronous send of data.");
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
      poller_->wait_for_read<tcp::socket>(socket_,
                                          std::bind(&client::on_receive, this));
    } else {
      __DISPLAY_ERROR__(
          "tcp::client::async_send: You must provide a callback in order to "
          "perform an asynchronous receive of data.");
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

  // A queue containing send requests.
  std::queue<std::pair<std::vector<char>, async_send_callback>> send_requests_;

  // A queue containing receive requests.
  std::queue<std::pair<std::size_t, async_receive_callback>> receive_requests_;
};

// TCP server.
class server {
 public:
  server(void) : running_(false), poller_(get_poller()) {}

  server(const server &) = delete;

  server &operator=(const server &) = delete;

  ~server(void) { stop(); }

 public:
  // Returns true or false whether the server is running.
  bool is_running(void) const { return running_; }

  // This function provides a callback that the server stores and will execute
  // on a new connection.
  void on_connection(
      const std::function<void(const std::shared_ptr<client> &)> &callback) {
    callback_ = callback;
  }

  // Runs the server on the giben host and port.
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
    poller_->add<tcp::socket>(socket_);
    poller_->wait_for_read<tcp::socket>(socket_,
                                        std::bind(&server::on_accept, this));
    running_ = true;
  }

  // Stop the server.
  void stop(void) {
    if (!running_) return;

    std::unique_lock<std::mutex> lock(mutex_);
    running_ = false;
    poller_->remove<tcp::socket>(socket_);
    socket_.close();
    for (auto &client : clients_) client->disconnect();
    clients_.clear();
  }

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
class client {
 public:
  client(void) : poller_(get_poller()) {}
  ~client() {}

 private:
  // Client's socket.
  socket socket_;

  // A smart pointer on the polling instance.
  std::shared_ptr<poller> poller_;
};

// UDP server.
class server {
 public:
  server(void) : poller_(get_poller()) {}
  ~server(void) {}

 private:
  // Server's socket.
  socket socket_;

  // A smart pointer on the polling instance.
  std::shared_ptr<poller> poller_;
};

}  // namespace udp
}  // namespace network
}  // namespace hermes
