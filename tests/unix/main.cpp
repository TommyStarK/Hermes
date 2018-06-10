#define CATCH_CONFIG_MAIN
#include "hermes.hpp"
#include "catch.hpp"

using namespace hermes::network;

TEST_CASE("UNIX TCP socket", "[unix][tcp][socket]") {
  SECTION("Should have default value for a default unix tcp socket") {
    tcp::socket s;

    CHECK(s.fd() == -1);
    CHECK(s.host() == "");
    CHECK(s.bound() == false);
    CHECK(s.port() == 0);
  }
}

