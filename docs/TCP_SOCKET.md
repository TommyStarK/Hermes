## TCP Socket:

Basic abstraction of the TCP socket features for unix and windows socket. The TCP socket provides blocking stream-oriented socket functionalities.


### public API:


```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::tcp;

  // Default constructor.
  socket(void);

  // Create a socket from an existing file descriptor.
  socket(int fd, const std::string &host, unsigned int port);

  // Copy constructor
  socket(socket &socket) = delete;

  // Assignment operator.
  socket &operator=(const socket &socket) = delete;

  // Move constructor.
  socket(socket&& socket);

  // Comparison operator.
  bool operator==(const socket &socket) const;

  //
  // Basic operations
  //

  // Returns the file descriptor associated to the socket.
  int get_fd(void) const;

  // Returns the host associated to the socket.
  const std::string &get_host(void) const;

  // Returns the port associated to the socket.
  unsigned int get_port(void) const;

  // Returns true if the socket is connected, false otherwise.
  bool is_socket_bound(void) const;


  //
  // Server operations
  //

  // Assign a name to the socket.
  void bind(const std::string &host, unsigned int port);

  // Mark the socket as a passive socket.
  void listen(unsigned int backlog = hermes::tools::BACKLOG);

  // Accept a new connection.
  socket accept(void);

  //
  // Client operations
  //

  // Connect to the given host and port.
  void connect(const std::string &host, unsigned int port);

  // Send data.
  void send(const std::string &data);
  void send(const std::vector<char> &data, std::size_t size);

  // Receive data.
  std::vector<char> receive(std::size_t size_to_read = hermes::tools::BUFFER_SIZE);

  //
  // Common operations.
  //

  // Close the file descriptor associated to the socket.
  void close(void);

```
