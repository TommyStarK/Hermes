## TCP Client

The TCP client class allows to create and use an asynchronous client. The client is using the polling model to detect
when the socket is ready for read or write data. Callbacks must be provided for each asynchronous operations.


### public API:

```cpp
  #include "hermes.hpp"

  using namespace hermes::network::tcp;

  typedef std::function<void(bool &, std::vector<char> &)> async_read_callback_t;
  typedef std::function<void(bool &, std::size_t &)> async_write_callback_t;

  // Default constructor.
  client(void);

  // Move contructor.
  explicit client(socket &&socket);

  // Copy consructor.
  client(const client &) = delete;

  // Assignment operator.
  client &operator=(const client &) = delete;



  // Connects the client to the given host and service.
  void connect(const std::string &host, unsigned int port);

  // Asynchronous receive of data.
  void async_read(const std::size_t &size, const async_read_callback_t &callback);

  // Asynchronous send of data.
  void async_write(const std::string &data, const async_write_callback_t &callback);
  void async_write(const std::vector<char> &data, const async_write_callback_t &callback);


  // Disconnects the client.
  void disconnect();



  // Returns the host to which the client is connected.
  const std::string &host(void) const;

  // Returns the client's service.
  const std::shared_ptr<hermes::internal::io_service> &io_service(void) const;

  // Returns true or false whether the client is connected.
  bool is_connected(void) const;

  // Returns the service to which the client is connected.
  unsigned int port(void) const;

  // Returns the client's socket.
  const socket &get_socket(void) const;

```


### example : Asynchronous TCP echo client.


```cpp
  #include "hermes.hpp"
  using namespace hermes::network;

  void on_read(tcp::client& client, bool& success, std::vector<char>& buffer) {
    if (success) {
      std::cout << buffer.data();
      client.async_write(buffer, nullptr);
      client.async_read(4096, std::bind(&on_read, std::ref(client), std::placeholders::_1, std::placeholders::_2));
    } else {
      std::cout << "client disconnecting...\n";
      client.disconnect();
    }
  }

  int main(void) {
    tcp::client client;

    try {
      client.connect("127.0.0.1", 27017);
      client.async_read(4096, std::bind(&on_read, std::ref(client), std::placeholders::_1, std::placeholders::_2));
    } catch(const std::exception& e) {
      std::cerr << e.what() << '\n';
      return 1;
    }

    hermes::signal::wait_for(SIGINT);
    return 0;
  }

```
