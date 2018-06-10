## TCP Server


The TCP server class allows to create and use an asynchronous server. The server is using the polling model to detect when
a client is trying to connect to the server. Before running the server, you must provide a callback to execute in case of connection.


### public API:


```cpp
  #include "hermes.hpp"

  using namespace hermes::network::tcp;

  // Default constructor.
  server(void);

  // Copy constructor.
  server(const server &server) = delete;

  // Move constructor.
  server(server&& server) = delete;

  // Assignment operator.
  server &operator=(const server &server) = delete;

  // Returns true or false whether the server is already running.
  bool is_running(void) const;

  // Returns the server's clients.
  const std::list<std::shared_ptr<client> > &clients(void) const;

  // Returns the server's service.
  const std::shared_ptr<hermes::internal::io_service> &io_service(void) const;

  // Returns the server's socket.
  const socket &socket(void) const;

  // Provides the callback which will be executed on a new connection. Represents the server behavior.
  // A callback must be provided using the 'on_connection' method before running the server.
  void on_connection(const std::function<void(const std::shared_ptr<client> &)> &callback);

  // Runs the server on the given host an service.
  void run(const std::string &host, unsigned int port, unsigned int max_conn = hermes::internal::MAX_CONN);

  // Stops the server.
  // Method called in the server's destructor.
  void stop(void);
```

### example: Asynchronous TCP echo server.


```cpp
  #include "hermes.hpp"

  using namespace hermes::network;

  void on_read(const std::shared_ptr<tcp::client>& client, bool& success, std::vector<char>& buffer) {
    if (success) {
      client->async_write(buffer, nullptr);
      client->async_read(4096, std::bind(&on_read, client, std::placeholders::_1, std::placeholders::_2));
    } else {
      std::cout << "client disconnecting...\n";
      client->disconnect();
    }
  }

  int main(void) {
    tcp::server server;
    
    try {
      server.on_connection([](const std::shared_ptr<tcp::client>& client) {
        client->async_read(4096, std::bind(&on_read, client, std::placeholders::_1, std::placeholders::_2));
      });

      server.run("127.0.0.1", 27017);
    } catch(const std::exception& e) {
      std::cerr << e.what() << '\n';
      return 1;
    }
    
    hermes::signal::wait_for(SIGINT);
    return 0;
  }

```