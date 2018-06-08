#define CATCH_CONFIG_MAIN
#include "hermes.hpp"
#include "catch.hpp"

using namespace hermes::network;

TEST_CASE("UNIX TCP socket", "[unix][tcp][socket]") {
  SECTION("Should have default value for a default unix tcp socket") {
    tcp::socket socket;

    CHECK(socket.fd() == -1);
    CHECK(socket.host() == "");
    CHECK(socket.bound() == false);
    CHECK(socket.port() == 0);
  }
}

