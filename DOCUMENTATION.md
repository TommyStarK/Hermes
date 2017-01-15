# Hermes documentation


Hermes is a lightweight, cross-platform, asynchronous, C++11 network library. Hermes provides an user-friendly API with which
you can easily create server or client following either TCP or UDP protocol. Hermes is based on a polling model using a thread
pool in order to offer an asynchronous I/O model to handle events between the server and the connected clients. Server and
client work asynchronously and the public Hermes API allows only to perform asynchronous operations.

To use Hermes, you just need to include the `Hermes.hpp` header in your code.

- Compiling using g++:


Assuming you want to compile a file containing some code using Hermes features. You just need to run the following command:

```bash
  g++ -std=c++11 your_file.cpp -pthread -o binary_name
```

# Summary:

- TCP API

  - Socket
    - public API

  - Server
    - public API
    - example: Asynchronous TCP echo server

  - Client
    - public API
    - example: Asynchronous TCP echo client


- UDP API

  - Socket
    - public API

  - Server
    - public API
    - example: Hello world!

  - Client
    - public API
    - example: Hello world!



## Hermes TCP API


Thanks to the Hermes TCP API, you can easily create either TCP server or TCP client.

### Socket:

Basic abstraction of the TCP socket features for unix and windows socket. The TCP socket is able to perform the basic server-side
and client-side operations such as binding the socket and listening on it for the server, or connecting the socket to a given
host/service for the client. Every operation of the TCP socket is synchronous.


#### public API:


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


### Server


The TCP server class allows to create and use an asynchronous server. The server is using the polling model to detect when
a client is trying to connect to the server. Before running the server, you must provide a callback to execute in case of connection.


#### public API:


```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::tcp;

  // Default constructor.
  server(void);

  // Copy constructor.
  server(const server &server) = delete;

  // Assignment operator.
  server &operator=(const server &server) = delete;

  // Returns true or false whether the server is already running.
  bool is_running(void) const;

  // Provides the callback which will be executed on a new connection. Represents the server behavior.
  // A callback must be provided using the 'on_connection' method before running the server.
  void on_connection(const std::function<void(const std::shared_ptr<client> &)> &callback);

  // Run the server on the given host an service.
  void run(const std::string &host, unsigned int port);

  // Stop the server.
  // Method called in the server's destructor.
  void stop(void);
```

#### example: Asynchronous TCP echo server.


```cpp
  #include "Hermes.hpp"
  using namespace hermes::network::tcp;

  void send_callback(const std::shared_ptr<client> &client, bool success, std::size_t bytes_sent) {
    if (success)
      std::cout << bytes_sent << std::endl;
    else
      client->disconnect();
  }

  void receive_callback(const std::shared_ptr<client> &client, bool success, std::vector<char> buffer) {
    if (success) {
      std::cout << buffer.data();
      client->async_send(std::string(buffer.data()), std::bind(&send_callback, client, std::placeholders::_1, std::placeholders::_2));
    } else {
      client->disconnect();
    }
  }

  int main(void) {
    server server;

    server.on_connection([](const std::shared_ptr<client> &client) {
      client->async_receive(1024, std::bind(&receive_callback, client, std::placeholders::_1, std::placeholders::_2));
    });

    server.run("127.0.0.1", 27017);

    // The calling thread will block until the specified signal is caught.
    // @param : int signal_number
    //
    hermes::tools::wait_for_signal(SIGINT);

    return 0;
  }

```

### Client

The TCP client class allows to create and use an asynchronous client. The client is using the polling model to detect
when the socket is ready for read or write data. Callbacks must be provided for each asynchronous operations.


#### public API:

- incoming:
  - asynchronous connection.
  - disconnection callback.

```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::tcp;

  // The callback executed when a send operation has been performed.
  typedef std::function<void(bool, std::size_t)> async_send_callback;
  // the callback executed when a receive operation has been performed.
  typedef std::function<void(bool, std::vector<char>)> async_receive_callback;

  // Default constructor.
  client(void);

  // Copy constructor.
  client(const client &client) = delete;

  // Assignment operator.
  client &operator=(const client &client) = delete;

  // Move constructor.
  client(socket&& socket);

  // Returns true or false whether the client is connected.
  bool is_connected(void) const;

  // Returns the client's socket.
  const socket &get_socket(void) const;

  // Connect the client to the given host and service.
  void connect(const std::string &host, unsigned int port);

  // Asynchronous send of data.
  void async_send(const std::string &data, const async_send_callback &callback);
  void async_send(const std::vector<char> &data, const async_send_callback &callback);

  // Asynchronous receive of data.
  void async_receive(std::size_t size_to_read, const async_receive_callback &callback);

  // Disconnect the client.
  // Method called in the client's destructor.
  void disconnect();

```


#### example : Asynchronous TCP echo client.


```cpp
  #include "Hermes.hpp"
  using namespace hermes::network::tcp;

  void receive_callback(client &client, bool success, std::vector<char> buffer) {
    if (success)
      std::cout << buffer.data();
    else
      client.disconnect();
  }

  void send_callback(client &client, bool success, std::size_t bytes_sent) {
    if (success) {
      std::cout << bytes_sent << std::endl;
      client.async_receive(1024, std::bind(&receive_callback, std::ref(client), std::placeholders::_1, std::placeholders::_2));
    } else
        client.disconnect();  
  }


  int main(void) {
    client client;

    client.connect("127.0.0.1", 27017 );
    client.async_send("Hello world!\n", std::bind(&send_callback, std::ref(client), std::placeholders::_1, std::placeholders:: _2));

    // The calling thread will block until the specified signal is caught.
    // @param : int signal_number
    //
    hermes::tools::wait_for_signal(SIGINT);

    return 0;
  }

```


## Hermes UDP API


An easy way to create and use UDP server or client.


### Socket


Basic abstraction of the UDP socket features for unix and windows socket. The UDP socket is able to perform the basic server-side
and client-side operations such as binding the socket and waiting for incoming packet on a specific host/port or sending/broadcasting
packets to host(s)/port.


#### public API:


```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::udp;

  // Default constructor.
  socket(void);

  // Copy constructor
  socket(socket &socket) = delete;

  // Assignment operator.
  socket &operator=(const socket &socket) = delete;

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

  // Returns true if the socket is bound, false otherwise.
  bool is_socket_bound(void) const;


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
  // Set broadcast_mode to true if you want to broadcast packets to various machine.
  void init(const std::string &host, unsigned int port, bool, broadcast_mode);

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

### Server


The UDP server class allows to create and use an asynchronous server waiting for incoming packets on a host/port.
The server must be bound to a given host/port before being able to wait for incoming packets. A callback must be
provided to the 'async_recvfrom' function, it represents the server behavior when it receives data.


#### public API:


```cpp
#include "Hermes.hpp"

// The callback executed when the server receives incoming packets.
typedef std::function<void(std::vector<char>, int)> async_receive_callback;

// Default constructor.
server(void);

// Copy constructor.
server(const server &server) = delete;

// Assignment operator.
server &operator=(const server &server) = delete;

// Returns true if the server is currently running, false otherwise.
bool is_running(void) const;

// Returns the server's socket.
const socket &get_socket(void) const;

// Assign a name to the socket with the given host/port.
void bind(const std::string &host, unsigned int port);

// Asynchronous receive of data.
void async_recvfrom(const async_receive_callback &callback);

// Stop the server.
// Method called in the server's destructor.
void stop(void);

```


#### Example: "Hello world!"


```cpp
  #include "Hermes.hpp"
  
  using namespace hermes::network::udp;

  int main(void) {
    server server;

    // By passing an empty string we specify that we want to use our ip. Feel free to specify any ip.
    //
    server.bind("", 27017);

    server.async_recvfrom([](std::vector<char> buffer, int bytes_received) {
      std::cout << "bytes received: " << buffer.data();
      std::cout << "number bytes received: " << bytes_received << std::endl;
    });

    // The calling thread will block until the specified signal is caught.
    // @param : int signal_number
    //
    hermes::tools::wait_for_signal(SIGINT);

    return 0;
  }
```


### client


The UDP client allows to create and use an asynchronous client. The UDP client can send packets to a given host/port
or broadcast them to many machines. A callback must be provided to use the 'async_send' or 'async_broadcast' methods.
If you enabled the broadcast mode, please use the 'async_broadcast' method to broadcast packets.


#### public API


```cpp
  #include "Hermes.hpp"

  #using namespace hermes::network::udp;


  // The callback executed when a send operation has been performed.
  typedef std::function<void(int)> async_send_callback;

  // Default constructor.
  client(void);

  // Copy constructor.
  client(const client &client) = delete;

  // Assignment operator.
  client &operator=(const client &client) = delete;

  // Returns true if the broadcast mode is enabled, false otherwise.
  bool broadcast_mode_enabled(void) const;

  // Returns the client's socket.
  const socket &get_socket(void) const;

  // Initialize the client with the given host/port. Set the boolean to true if you want
  // to broadcast packets to various machine, false otherwise.
  void init(const std::string &host, unsigned int port, bool broadcast_mode);

  // Asynchronous send of data.
  void async_send(const std::string &str, const async_send_callback &callback);
  void async_send(const std::vector<char> &data, const async_send_callback &callback);

  // Asynchronous broadcast of data.
  void async_broadcast(const std::string &str, const async_send_callback &callback);
  void async_broadcast(const std::vector<char> &data, const async_send_callback &callback);

  // Stop the client.
  // Method called in the client's destructor.
  void stop(void);
```


#### Example: "Hello world!"


```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::udp;

  int main(void) {
    client client;

    // We initialize our client with an host and a port and specifying to
    // disable the broadcast mode.
    //
    // If you want to enable the broadcast mode, set the boolean to true and use the function 'async_broadcast' instead of 'async_send'.
    //
    client.init("127.0.0.1", 27017, false);

    client.async_send("Hello world!\n", [](int bytes_sent) {
      std::cout << "Number of bytes sent: " << bytes_sent << std::endl;
    });

    // The calling thread will block until the specified signal is caught.
    // @param : int signal_number
    //
    hermes::tools::wait_for_signal(SIGINT);

    return 0;
  }

```
