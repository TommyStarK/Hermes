#define CATCH_CONFIG_MAIN
#include "Netlib.hpp"
#include "catch.hpp"

using namespace netlib;

SCENARIO("test tcp socket") {
  network::tcp::socket socket;

  REQUIRE(socket.get_fd() == -1);
  REQUIRE(socket.get_host() == "");
  REQUIRE(socket.get_port() == 0);
}
