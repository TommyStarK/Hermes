## TCP Client

The TCP client class allows to create and use an asynchronous client. The client is using the polling model to detect
when the socket is ready for read or write data. Callbacks must be provided for each asynchronous operations.


### public API:

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


### example : Asynchronous TCP echo client.


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
    hermes::tools::signal::wait_for(SIGINT);

    return 0;
  }

```
