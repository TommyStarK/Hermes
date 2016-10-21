#define CATCH_CONFIG_MAIN
#include "Netlib.hpp"
#include "catch.hpp"

using namespace netlib;

SCENARIO("Test boost headers-only/linked library and Catch") {
  auto test1 = boost::bind(&simple_test_1, "test");
  auto test2 = simple_test_2();

  std::cout << test1() << "\n";
  REQUIRE(test1() == "test");
  std::cout << std::boolalpha << test2 << "\n";
  REQUIRE(test2 == false);
}
