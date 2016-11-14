#define CATCH_CONFIG_MAIN
#include "Netlib.hpp"
#include "catch.hpp"

using namespace netlib;

SCENARIO("simple test") {
  REQUIRE(network::tcp::simple_test_1("toto") == "toto");
}
