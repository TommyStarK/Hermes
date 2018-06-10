#include "include/hermes.hpp"

using namespace hermes::network;

void on_read(tcp::client& client, bool& success, std::vector<char>& buffer) {
  if (success) {
    std::cout << buffer.data();
    client.async_write(buffer, nullptr);
    client.async_read(4096, std::bind(&on_read, std::ref(client), std::placeholders::_1, std::placeholders::_2));
  } else {
    std::cout << "client disconnecting...\n";
    client.disconnect();
  }
}

int main(void) {
  tcp::client client;
  
  try {
    client.connect("127.0.0.1", 27017);
    client.async_read(1024, std::bind(&on_read, std::ref(client), std::placeholders::_1, std::placeholders::_2));
  } catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  hermes::signal::wait_for(SIGINT);
  return 0;
}

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