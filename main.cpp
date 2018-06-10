#include "include/hermes.hpp"

#include <unistd.h>
#include <iostream>
#include <memory>

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

    server.run("127.0.0.1", 27017, 50);
  } catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  
  hermes::signal::wait_for(SIGINT);
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