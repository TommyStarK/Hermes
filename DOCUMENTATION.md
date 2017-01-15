# Hermes documentation


Hermes is a lightweight, cross-platform, asynchronous, C++11 network library. Hermes provides an user-friendly API with which
you can easily create server or client following either TCP or UDP protocol. Hermes is based on a polling model using a thread
pool in order to offer an asynchronous I/O model to handle events between the server and the connected clients. Server and
client work asynchronously and the public Hermes API allows only to perform asynchronous operations.
To use Hermes, you just need to include the `Hermes.hpp` header in your code.


- compiling using g++:


Assuming you want to compile a file containing some code using Hermes features. You just need to run the following command:

```bash
  g++ -std=c++11 your_file.cpp -pthread -o binary_name
```


## Hermes TCP API


Thanks to the Hermes TCP API, you can easily create either TCP server or TCP client.

### Socket:

Basic abstraction of the TCP socket features for unix and windows socket. The TCP socket is able to perform the basic server-side
and client-side operations such as binding the socket and listening on it for the server, or connecting the socket to a given
host/service for the client. Every operation of the TCP socket is synchronous.

```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::tcp;

  // Default constructor.
  socket(void);

  // Create a socket from an existing file descriptor.
  socket(int fd, const std::string &host, unsigned int port);

  // Move constructor.
  socket(socket&& socket);

  // operator ==
  bool operator==(const socket &socket) const;

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
  void send(std::vector<char> data, std::size_t size);

  // Read data.
  std::vector<char> receive(std::size_t size_to_read = hermes::tools::BUFFER_SIZE);

  //
  // Common operations.
  //

  // Close the file descriptor associated to the socket.
  void close(void);

```


### Server


The TCP server class allows to create and use an asynchronous server. The server is using the polling model to detect when a client
is trying to connect to the server. Before running the server, you must provide a callback to execute incase of connection.


#### Methods:


```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::tcp;

  // Default constructor.
  server(void);

  // Returns true or false whether the server is already running.
  bool is_running(void) const;

  // Provide the callback which will be executed on a new connection. Represents the server behavior.
  // A callback must be provided using the 'on_connection' method before running the server.
  void on_connection(const std::function<void(const std::shared_ptr<client> &)> &callback);

  // Run the server on the given host an service.
  void run(const std::string &host, unsigned int port);

  // Stop the server.
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


#### Methods:

- incoming:
  - asynchronous connection.
  - disconnection callback.

```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::tcp;

  typedef std::function<void(bool, std::size_t)> async_send_callback;
  typedef std::function<void(bool, std::vector<char>)> async_receive_callback;
  
  // Default constructor.
  client(void);

  // Move constructor.
  client(socket&& socket);

  // Returns true or false whether the client is connected.
  bool is_connected(void) const;

  // Returns the client's socket.
  const socket &get_socket(void) const;

  // Connect the client to the given host and service.
  void connect(const std::string &host, unsigned int port);

  // Asynchronous send operation.
  void async_send(const std::string &data, const async_send_callback &callback);
  void async_send(std::vector<char> data, const async_send_callback &callback);

  // Asynchronous receive operation.
  void async_receive(std::size_t size_to_read, const async_receive_callback &callback);

// Disconnect the client.
  // Disconnect method is call in the destructor.
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
