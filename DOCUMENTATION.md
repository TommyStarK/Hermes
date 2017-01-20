# Hermes documentation


Hermes is a lightweight, cross-platform, asynchronous, C++11 network library. Hermes provides an user-friendly API with which
you can easily create a server or client following either the TCP or UDP protocol. Hermes is based on a polling model using a
thread pool to handle events asynchronously between the server and the clients.

To use Hermes, you just need to include the `Hermes.hpp` header in your code.

- Compiling using g++:


Assuming you want to compile a file containing some code using Hermes' features. You just need to run the following command:

```bash
  g++ -std=c++11 your_file.cpp -pthread -o binary_name
```

# Summary:

- TCP API

  - [Socket](docs/TCP_SOCKET.md)
    - public API

  - [Server](docs/TCP_SERVER.md)
    - public API
    - example: Asynchronous TCP echo server

  - [Client](docs/TCP_CLIENT.md)
    - public API
    - example: Asynchronous TCP echo client


- UDP API

  - [Socket](docs/UDP_SOCKET.md)
    - public API

  - [Server](docs/UDP_SERVER.md)
    - public API
    - example: Hello world!

  - [Client](docs/UDP_CLIENT.md)
    - public API
    - example: Hello world!
