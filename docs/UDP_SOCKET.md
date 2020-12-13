# UDP Socket

Basic abstraction of the UDP socket features for unix and windows socket. The UDP socket provides blocking datagram-oriented socket functionalities.

## API

```cpp
  #include "hermes.hpp"

  using namespace hermes::network::udp;

  // Default constructor.
  socket(void);

  // Copy constructor.
  socket(socket &socket) = delete;

  // Assignment operator.
  socket &operator=(const socket &socket) = delete;

  // Comparison operator.
  bool operator==(const socket &socket) const;

  //
  // Basic operations
  //

  // Returns true if the socket has a name assigned, false otherwise.
  bool bound(void) const;

  // Returns the file descriptor associated to the socket.
  int fd(void) const;

  // Returns the host associated to the socket.
  const std::string &host(void) const;

  // Returns the port associated to the socket.
  unsigned int port(void) const;


  //
  // Server operations
  //

  // Assign a name to the socket.
  void bind(const std::string &host, unsigned int port);

  // Receive data.
  std::size_t recvfrom(std::vector<char> &buffer);

  //
  // Client operations
  //

  // Initialize the udp client.
  // Set broadcast_mode to true if you want to broadcast packets to several machines.
  void init(const std::string &host, unsigned int port, bool broadcast_mode);

  // Send data.
  std::size_t sendto(const std::string &str);
  std::size_t sendto(const std::vector<char> &data, std::size_t size);

  // Broadcast data.
  std::size_t broadcast(const std::string &str);
  std::size_t broadcast(const std::vector<char> &data, std::size_t size);

  //
  // Common operations.
  //

  // Close the file descriptor associated to the socket.
  void close(void);
```
