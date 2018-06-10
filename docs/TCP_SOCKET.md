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
  int fd(void) const;

  // Returns the host associated to the socket.
  const std::string &host(void) const;

  // Returns the port associated to the socket.
  unsigned int port(void) const;

  // Returns true if the socket has a name assigned, false otherwise.
  bool bound(void) const;

  //
  // Server operations
  //

  // Assigns a name to the socket.
  void bind(const std::string &host, unsigned int port);

  // Marks the socket as a passive socket.
  void listen(unsigned int backlog = hermes::internal::MAX_CONN);

  // Accepts a new connection.
  socket accept(void);

  //
  // Client operations
  //

  // Connects to the given host and port.
  void connect(const std::string &host, unsigned int port);

  // Sends data.
  void send(const std::string &data);
  void send(const std::vector<char> &data, std::size_t size);

  // Receives data.
  std::vector<char> receive(std::size_t size_to_read = hermes::tools::BUFFER_SIZE);

  //
  // Common operations.
  //

  // Closes the file descriptor associated to the socket.
  void close(void);

```
