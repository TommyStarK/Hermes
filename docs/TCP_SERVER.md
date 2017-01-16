## TCP Server


The TCP server class allows to create and use an asynchronous server. The server is using the polling model to detect when
a client is trying to connect to the server. Before running the server, you must provide a callback to execute in case of connection.


### public API:


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

  // Returns the server's socket.
  const socket &get_socket(void) const = delete;

  // Provides the callback which will be executed on a new connection. Represents the server behavior.
  // A callback must be provided using the 'on_connection' method before running the server.
  void on_connection(const std::function<void(const std::shared_ptr<client> &)> &callback);

  // Run the server on the given host an service.
  void run(const std::string &host, unsigned int port);

  // Stop the server.
  // Method called in the server's destructor.
  void stop(void);
```

### example: Asynchronous TCP echo server.


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
