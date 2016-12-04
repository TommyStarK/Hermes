#define CATCH_CONFIG_MAIN
#include <chrono>
#include <thread>
#include "Netlib.hpp"
#include "catch.hpp"

using namespace netlib;

// TCP socket tests
SCENARIO("testing tcp socket default ctor") {
  network::tcp::socket socket;
  network::tcp::socket socket2;

  REQUIRE(socket.get_fd() == -1);
  REQUIRE(socket.get_host() == "");
  REQUIRE(socket.get_port() == 0);
  REQUIRE(socket == socket2);
}

SCENARIO("testing tcp socket: server operations") {
  GIVEN("default tcp sockets") {
    network::tcp::socket default_socket;
    network::tcp::socket socket_for_test;

    WHEN("assigning a name to the socket (bind)") {
      REQUIRE(socket_for_test == default_socket);
      REQUIRE_NOTHROW(socket_for_test.bind("127.0.0.1", 27017));
      REQUIRE(socket_for_test.get_fd() != -1);
      REQUIRE(socket_for_test.get_host() == "127.0.0.1");
      REQUIRE(socket_for_test.get_port() == 27017);
      REQUIRE(socket_for_test.is_socket_bound() == true);
      REQUIRE((socket_for_test == default_socket) == false);
      REQUIRE_THROWS(socket_for_test.bind("127.0.0.1", 27017));
      REQUIRE_NOTHROW(socket_for_test.close());
    }

    WHEN("marking the socket as passive (listen)") {
      REQUIRE_THROWS(default_socket.listen(30));
      REQUIRE_NOTHROW(socket_for_test.bind("127.0.0.1", 27017));
      REQUIRE_NOTHROW(socket_for_test.listen());
      REQUIRE_NOTHROW(socket_for_test.close());
    }

    WHEN("accepting a new connection") {
      REQUIRE_THROWS(default_socket.accept());

      std::thread server([&socket_for_test]() {
        REQUIRE_NOTHROW(socket_for_test.bind("127.0.0.1", 27017));
        REQUIRE_NOTHROW(socket_for_test.listen());
        auto client = socket_for_test.accept();
        REQUIRE((socket_for_test == client) == false);
        REQUIRE(client.get_fd() != socket_for_test.get_fd());
        REQUIRE_NOTHROW(socket_for_test.close());
      });

      std::this_thread::sleep_for(std::chrono::seconds(1));

      std::thread client([&default_socket]() {
        REQUIRE_NOTHROW(default_socket.connect("127.0.0.1", 27017));
        REQUIRE_NOTHROW(default_socket.close());
      });

      REQUIRE_NOTHROW(server.join());
      REQUIRE_NOTHROW(client.join());
    }
  }
}

SCENARIO("testing tcp socket: client operations") {
  GIVEN("default tcp sockets") {
    network::tcp::socket default_socket;
    network::tcp::socket socket_for_test;

    WHEN("connecting to the given endpoint") {
      std::thread server([&default_socket]() {
        REQUIRE_NOTHROW(default_socket.bind("127.0.0.1", 27017));
        REQUIRE_NOTHROW(default_socket.listen());
        auto client = default_socket.accept();
        REQUIRE_NOTHROW(default_socket.close());
      });

      std::this_thread::sleep_for(std::chrono::seconds(1));

      std::thread client([&socket_for_test]() {
        REQUIRE_NOTHROW(socket_for_test.connect("127.0.0.1", 27017));
        REQUIRE_NOTHROW(socket_for_test.close());
      });

      REQUIRE_NOTHROW(server.join());
      REQUIRE_NOTHROW(client.join());
    }

    WHEN("sending/receiving amount of data") {
      std::thread server([&default_socket]() {
        REQUIRE_NOTHROW(default_socket.bind("127.0.0.1", 27017));
        REQUIRE_NOTHROW(default_socket.listen());
        auto client = default_socket.accept();
        std::string rcv(client.receive().data());
        REQUIRE(rcv == "test ok :)");
        REQUIRE(rcv.size() == 10);
        REQUIRE_NOTHROW(default_socket.close());
      });

      std::this_thread::sleep_for(std::chrono::seconds(1));

      std::thread client([&socket_for_test]() {
        REQUIRE_NOTHROW(socket_for_test.connect("127.0.0.1", 27017));
        auto bytes = socket_for_test.send("test ok :)");
        REQUIRE(bytes == 10);
        REQUIRE_NOTHROW(socket_for_test.close());
      });

      REQUIRE_NOTHROW(server.join());
      REQUIRE_NOTHROW(client.join());
    }
  }
}
