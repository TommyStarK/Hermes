## UDP Server


The UDP server class allows to create and use an asynchronous server waiting for incoming packets on a host/port.
The server must be bound to a given host/port before being able to wait for incoming packets. A callback must be
provided to the 'async_recvfrom' method, it represents the server behavior when it receives data.


### public API:


```cpp
#include "Hermes.hpp"

// The callback executed when the server receives data.
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


### Example: "Hello world!"


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
    hermes::tools::signal::wait_for(SIGINT);

    return 0;
  }
```
