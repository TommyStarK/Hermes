#include "boost/bind/bind.hpp"
#include "boost/asio.hpp"

#include <iostream>

int toto (int a, int b) {
  return a + b;
}

int main() {
  auto test = boost::bind(&toto, 1, 2);

  auto result = test();

  std::cout << result << std::endl;


  boost::asio::io_service service;
  boost::asio::ip::tcp::socket socket(service);

  auto res = socket.is_open();

  std::cout << std::boolalpha << res << std::endl;

  return 0;

}
