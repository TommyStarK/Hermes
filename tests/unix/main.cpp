#include "Hermes.hpp"

using namespace hermes;
using namespace hermes::network;

void tests_udp_socket(void) {
  udp::socket socket;

  assert(socket.get_fd() == -1);
  assert(socket.get_host() == "");
  assert(socket.get_port() == 0);
}

void tests_tcp_socket(void) {
  tcp::socket socket;

  assert(socket.get_fd() == -1);
  assert(socket.get_host() == "");
  assert(socket.get_port() == 0);
}

int __cdecl main(void) {
  tests_tcp_socket();
  tests_udp_socket();
  return 0;
}
