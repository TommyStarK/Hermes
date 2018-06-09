#include "include/hermes.hpp"

#include <unistd.h>
#include <iostream>
#include <memory>
#include <thread>

int main() {
  hermes::network::tcp::socket socket;
  auto io_service = hermes::internal::get_io_service(200);

  if (!io_service) {
    std::cout << "io_service should not be null\n";
    return 1;
  }

  try {
    socket.bind("127.0.0.1", 27017);
    socket.listen(10);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  std::cout << "listening on localhost:27017\n";
  io_service->subscribe<hermes::network::tcp::socket>(socket);
  io_service->on_read<hermes::network::tcp::socket>(socket, [&](int){
      auto client = std::make_shared<hermes::network::tcp::socket>(socket.accept());
      std::cout << "new client: " << client->fd() << std::endl;

      io_service->subscribe<hermes::network::tcp::socket>(*client);
      io_service->on_read<hermes::network::tcp::socket>(*client, [client, io_service](int){
        auto data = client->receive();
        std::cout << data.data() << std::endl;

        io_service->on_write<hermes::network::tcp::socket>(*client, [client, data](int){
          client->send(data.data());
        });
      });
  });
  hermes::internal::signal::wait_for(SIGINT);
  return 0;
}

// echo tcp server
// int main() {
//   std::mutex m;
//   hermes::internal::thread_pool tp(100);
//   hermes::network::tcp::socket s;

//   try {
//     s.bind("127.0.0.1", 27017);
//     s.listen(10);
//   } catch (const std::exception& e) {
//     std::cerr << e.what() << '\n';
//     return 1;
//   }

//    for(;;) {
//     try {

//       {
//         std::lock_guard<std::mutex> lock(m);
//         std::cout << "waiting for connections !!\n";
//       }

//       auto client =
//       std::make_shared<hermes::network::tcp::socket>(s.accept());

//       {
//         std::lock_guard<std::mutex> lock(m);
//         std::cout << "client accepted: " << client->fd() << std::endl;
//       }

//       tp.register_task([client, &m] {
//         while (client->fd() != -1) {
//           auto data = client->receive();

//           if (!data.size()) {
//             return;
//           }

//           {
//             std::lock_guard<std::mutex> lock(m);
//             std::cout << data.data();
//           }

//           client->send(data.data());
//         }

//         client->close();
//       });
//     } catch (const std::exception& e) {
//       std::cerr << e.what() << '\n';
//     }
//    }

//   return 0;
// }