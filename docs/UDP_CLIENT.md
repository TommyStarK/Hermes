## UDP client


The UDP client allows to create and use an asynchronous client. The UDP client can send packets to a given host/port
or broadcast them to many machines. A callback must be provided to use the 'async_send' or 'async_broadcast' methods.
If you enabled the broadcast mode, please use the 'async_broadcast' method to broadcast packets.


### public API


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


### Example: "Hello world!"


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
    hermes::tools::signal::wait_for(SIGINT);

    return 0;
  }

```


### Example: Broadcasting "Hello world!"



```cpp
  #include "Hermes.hpp"

  using namespace hermes::network::udp;

  int main(int ac, char **av) {
    client client;


    // check if an host has been provided as argument.
    if (ac != 2) {
       std::cerr << "[Usage]: ./binary_name host.\n";
       return 1;
    }

    // Initialize a client with broadcasting mode enabled.
    client.init(av[1], 27017, true);

    // Asynchronous broadcast of data.
    client.async_broadcast("Hello world!\n", [](int bytes_sent) {
    	std::cout << "Number of bytes sent: " << bytes_sent << std::endl;
    });

    // The calling thread will block until the specified signal is caught
    // @param: int signal_number.
    //
    hermes::tools::signal::wait_for(SIGINT);

    return 0;
  }
```
