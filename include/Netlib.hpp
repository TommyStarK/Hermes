#pragma once

#include <iostream>
#include <string>
#include "boost/asio.hpp"
#include "boost/bind/bind.hpp"

namespace netlib {

std::string simple_test_1(const std::string& s) { return std::string(s); }

bool simple_test_2() {
  boost::asio::io_service service;
  boost::asio::ip::tcp::socket socket(service);
  return socket.is_open();
}

}  // namespace netlib
