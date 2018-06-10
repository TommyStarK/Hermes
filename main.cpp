#include "include/hermes.hpp"

#include <unistd.h>
#include <iostream>
#include <memory>

// void
// on_new_message(const std::shared_ptr<hermes::network::tcp::client>& client, const hermes::network::tcp::client::read_result& res) {
//   if (res.success) {
//     client->async_write({res.buffer, nullptr});
//     client->async_read({1024, std::bind(&on_new_message, client, std::placeholders::_1)});
//   }
//   else {
//     std::cout << "Client disconnected" << std::endl;
//     client->disconnect();
//   }
// }

// int main(void) {
//   hermes::network::tcp::server s;
//   s.start("127.0.0.1", 27017, [](const std::shared_ptr<hermes::network::tcp::client>& client) -> bool {
//     client->async_read({1024, std::bind(&on_new_message, client, std::placeholders::_1)});
//     return true;
//   });

//   hermes::internal::signal::wait_for(SIGINT);
//   return 0;
// }

// echo tcp server
int main() {
  std::mutex m;
  hermes::internal::thread_pool tp(100);
  hermes::network::tcp::socket s;

  try {
    s.bind("127.0.0.1", 27017);
    s.listen(10);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

   for(;;) {
    try {

      {
        std::lock_guard<std::mutex> lock(m);
        std::cout << "waiting for connections !!\n";
      }

      auto client =
      std::make_shared<hermes::network::tcp::socket>(s.accept());

      {
        std::lock_guard<std::mutex> lock(m);
        std::cout << "client accepted: " << client->fd() << std::endl;
      }

      tp.register_task([client, &m] {
        while (client->fd() != -1) {
          auto data = client->receive();

          if (!data.size()) {
            return;
          }

          {
            std::lock_guard<std::mutex> lock(m);
            std::cout << data.data();
          }

          client->send(data.data());
        }

        client->close();
      });
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
    }
   }

  return 0;
}