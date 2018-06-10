#include "include/hermes.hpp"

using namespace hermes::network;

int main(int ac, char **av) {
  udp::client client;

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


// int main(void) {
//   udp::client client;

//   client.init("127.0.0.1", 27017, false);

//   client.async_send("Hello world!\n", [](int bytes_sent) {
//     std::cout << "Number of bytes sent: " << bytes_sent << std::endl;
//   });

//   hermes::signal::wait_for(SIGINT);
//   return 0;
// }

// int main(void) {
//   udp::server server;

//   server.bind("", 27017);

//   server.async_recvfrom([](std::vector<char> buffer, int bytes_received) {
//     std::cout << "bytes received: " << buffer.data();
//     std::cout << "number bytes received: " << bytes_received << std::endl;
//   });

//   hermes::signal::wait_for(SIGINT);
//   return 0;
// }

// void on_read(tcp::client& client, bool& success, std::vector<char>& buffer) {
//   if (success) {
//     std::cout << buffer.data();
//     client.async_write(buffer, nullptr);
//     client.async_read(4096, std::bind(&on_read, std::ref(client), std::placeholders::_1, std::placeholders::_2));
//   } else {
//     std::cout << "client disconnecting...\n";
//     client.disconnect();
//   }
// }

// int main(void) {
//   tcp::client client;
  
//   try {
//     client.connect("127.0.0.1", 27017);
//     client.async_read(1024, std::bind(&on_read, std::ref(client), std::placeholders::_1, std::placeholders::_2));
//   } catch(const std::exception& e) {
//     std::cerr << e.what() << '\n';
//     return 1;
//   }

//   hermes::signal::wait_for(SIGINT);
//   return 0;
// }

// void on_read(const std::shared_ptr<tcp::client>& client, bool& success, std::vector<char>& buffer) {
//   if (success) {
//     client->async_write(buffer, nullptr);
//     client->async_read(4096, std::bind(&on_read, client, std::placeholders::_1, std::placeholders::_2));
//   } else {
//     std::cout << "client disconnecting...\n";
//     client->disconnect();
//   }
// }

// int main(void) {
//   tcp::server server;
  
//   try {
//     server.on_connection([](const std::shared_ptr<tcp::client>& client) {
//       client->async_read(4096, std::bind(&on_read, client, std::placeholders::_1, std::placeholders::_2));
//     });

//     server.run("127.0.0.1", 27017, 50);
//   } catch(const std::exception& e) {
//     std::cerr << e.what() << '\n';
//     return 1;
//   }
  
//   hermes::signal::wait_for(SIGINT);
//   return 0;
// }