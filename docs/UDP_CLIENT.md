## UDP client


The UDP client allows to create and use an asynchronous client. The UDP client can send packets to a given host/port
or broadcast them to many machines. A callback must be provided to use the 'async_send' or 'async_broadcast' methods.
If you enabled the broadcast mode, please use the 'async_broadcast' method to broadcast packets.


### public API


```cpp
  #include "hermes.hpp"

  #using namespace hermes::network::udp;


  // The callback executed when a send operation has been performed.
  typedef std::function<void(int)> async_send_callback_t;

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
  void async_send(const std::string &str, const async_send_callback_t &callback);
  void async_send(const std::vector<char> &data, const async_send_callback_t &callback);

  // Asynchronous broadcast of data.
  void async_broadcast(const std::string &str, const async_send_callback_t &callback);
  void async_broadcast(const std::vector<char> &data, const async_send_callback_t &callback);

  // Stop the client.
  // Method called in the client's destructor.
  void stop(void);
```


### Example: "Hello world!"


```cpp
  #include "hermes.hpp"

  using namespace hermes::network::udp;

  int main(void) {
    client client;

    client.init("127.0.0.1", 27017, false);

    client.async_send("Hello world!\n", [](int bytes_sent) {
      std::cout << "Number of bytes sent: " << bytes_sent << std::endl;
    });

    hermes::signal::wait_for(SIGINT);
    return 0;
  }

```


### Example: Broadcasting "Hello world!"



```cpp
  #include "hermes.hpp"

  using namespace hermes::network::udp;

  int main(int ac, char **av) {
    client client;

    if (ac != 2) {
       std::cerr << "[Usage]: ./binary_name host.\n";
       return 1;
    }

    client.init(av[1], 27017, true);

    client.async_broadcast("Hello world!\n", [](int bytes_sent) {
    	std::cout << "Number of bytes sent: " << bytes_sent << std::endl;
    });

    hermes::signal::wait_for(SIGINT);
    return 0;
  }
```
