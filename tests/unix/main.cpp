#define CATCH_CONFIG_MAIN
#include "hermes.hpp"
#include "catch.hpp"

using namespace hermes::network;

void display(const std::string &to_display) {
  std::cout << to_display + " [\x1b[32;1mOK\x1b[0m]\n";
}


//
// Tests: TCP socket.
//
TEST_CASE("TCP socket tests", "[network][tcp][socket][default]") {
  SECTION("Default TCP socket should have default data") {
    tcp::socket socket1;
    tcp::socket socket2;

    CHECK(socket1.fd() == -1);
    CHECK(socket1.host() == "");
    CHECK(socket1.port() == 0);
    CHECK(socket1.bound() == false);

    THEN("The two default TCP sockets should be equal") {
      CHECK(socket1 == socket2);
    }
    display("[TCP socket] Testing default socket");
  }

  SECTION("Default TCP socket should have default behavior") {
    tcp::socket socket;

    REQUIRE_THROWS_AS(socket.listen(10), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.accept(), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.send(""), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.receive(1024), std::logic_error);
    display("[TCP socket] Testing default behavior");
  }
}