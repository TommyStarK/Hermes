#define CATCH_CONFIG_MAIN
#include "hermes.hpp"
#include "catch.hpp"

using namespace hermes;
using namespace hermes::network;

void display(const std::string &to_display) {
  std::cout << to_display + " [\x1b[32;1mOK\x1b[0m]\n";
}

//
// Tests: Thread pool.
//
TEST_CASE("Thread pool tests", "[internal][thread_pool]") {
  SECTION("Should process the registered tasks correctly") {
    internal::thread_pool pool(10);

    pool.register_task(nullptr);
    pool.register_task([]() { std::cout << "processing: job 1\n"; });
    pool.register_task([]() { std::cout << "processing: job 2\n"; });
    pool.register_task([]() { std::cout << "processing: job 3\n"; });
    pool.register_task([]() { std::cout << "processing: job 4\n"; });
    pool.register_task([]() { std::cout << "processing: job 5\n"; });

    pool.stop();
    display("[internal][thread_pool] Testing default behavior");
  }
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

    display("[TCP][socket] Testing default socket");
  }

  SECTION("Default TCP socket should have default behavior") {
    tcp::socket socket;

    REQUIRE_THROWS_AS(socket.listen(10), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.accept(), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.send(""), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.receive(1024), std::logic_error);
    display("[TCP][socket] Testing default behavior");
  }
}

TEST_CASE("TCP socket tests server operations",
          "[network][tcp][socket][operations][server]") {
  SECTION("Should assign a name to the socket") {
    tcp::socket socket1;
    tcp::socket socket2;

    CHECK(socket1 == socket2);
    REQUIRE_NOTHROW(socket1.bind("127.0.0.1", 27017));

    THEN("tryng to bind the socket again should diplay a warning message") {
      CHECK_NOTHROW(socket1.bind("127.0.0.1", 27017));
    }

    REQUIRE(socket1.fd() != -1);
    CHECK(socket1.host() == "127.0.0.1");
    CHECK(socket1.port() == 27017);
    CHECK(socket1.bound());
    CHECK(!(socket1 == socket2));

    REQUIRE_NOTHROW(socket1.close());
    THEN("Should be equal to socket2") { CHECK(socket1 == socket2); }
  }

  SECTION("Should mark the socket as passive") {
    tcp::socket socket;

    REQUIRE_THROWS_AS(socket.listen(10), std::logic_error);
    REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
    CHECK(socket.bound());
    REQUIRE_NOTHROW(socket.listen(50));

    THEN("Trying to connect a socket marked as passive should throw an error") {
      REQUIRE_THROWS_AS(socket.connect("127.0.0.1", 27017), std::logic_error);
    }

    REQUIRE_NOTHROW(socket.close());

    THEN("Should reset the socket") {
      CHECK(socket.fd() == -1);
      CHECK(!socket.bound());
    }
  }

  SECTION("Should accept a new connection") {
    std::thread server([] {
      tcp::socket socket;

      REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
      REQUIRE_NOTHROW(socket.listen(10));
      auto client = std::make_shared<tcp::socket>(socket.accept());
      CHECK(!(socket == *client));
      REQUIRE_NOTHROW(socket.close());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread client([] {
      tcp::socket socket;

      REQUIRE_NOTHROW(socket.connect("127.0.0.1", 27017));
      REQUIRE_NOTHROW(socket.close());
    });

    REQUIRE_NOTHROW(server.join());
    REQUIRE_NOTHROW(client.join());
    display("[TCP][socket] Testing accept new connection");
  }
}

TEST_CASE("TCP socket tests client operations",
          "[network][tcp][socket][operations][client]") {
  SECTION("Should correctly send/receive data") {
    std::thread server([] {
      tcp::socket socket;

      REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
      REQUIRE_NOTHROW(socket.listen(10));
      auto client = std::make_shared<tcp::socket>(socket.accept());
      std::string data(client->receive().data());
      REQUIRE(data.size() == 13);
      REQUIRE(data == "Hello world!\n");
      REQUIRE_NOTHROW(socket.close());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread client([] {
      tcp::socket socket;

      REQUIRE_NOTHROW(socket.connect("127.0.01", 27017));
      auto bytes_sent = socket.send("Hello world!\n");
      REQUIRE(bytes_sent == 13);
      REQUIRE_NOTHROW(socket.close());
    });

    REQUIRE_NOTHROW(server.join());
    REQUIRE_NOTHROW(client.join());
    display("[TCP][socket] Testing send/receive data");
  }
}

//
// Tests: TCP server/client.
//
TEST_CASE("TCP server/client tests", "[network][tcp][server][client]") {
  SECTION("Default TCP server should have default behaviour") {
    tcp::server server;

    CHECK(!server.is_running());
    CHECK(server.get_socket().fd() == -1);
    CHECK(!server.get_socket().bound());
    CHECK_THROWS_AS(server.run("127.0.0.1", 27017), std::logic_error);
    display("[TCP][server] Default behavior");
  }

  SECTION("Default TCP client should have default behaviour") {
    tcp::client client;

    CHECK(!client.is_connected());
    CHECK(client.get_socket().fd() == -1);
    CHECK(!client.get_socket().bound());
    CHECK_THROWS_AS(client.async_write("", nullptr), std::logic_error);
    CHECK_THROWS_AS(client.async_read(1024, nullptr), std::logic_error);
    display("[TCP][client] Default behavior");
  }

  SECTION("Should correctly send and receive data") {
    tcp::server server;
    tcp::client client;

    std::thread thread_server([&server] {

      server.on_connection([](const std::shared_ptr<tcp::client> &client) {
        client->async_read(1024, [](bool success, std::vector<char> data) {
          REQUIRE(success);
          REQUIRE((unsigned)std::strlen(data.data()) == 13);
        });
      });

      REQUIRE_NOTHROW(server.run("127.0.0.1", 27017));
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      REQUIRE_THROWS_AS(server.run("127.0.0.1", 27017), std::logic_error);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread thread_client([&client] {

      REQUIRE_NOTHROW(client.connect("127.0.0.1", 27017));

      client.async_write("Hello world!\n", [](bool success, std::size_t sent) {
          REQUIRE(success);
          REQUIRE(sent == 13);
      });

      REQUIRE_THROWS_AS(client.connect("127.0.0.1", 27017), std::logic_error);
      client.disconnect();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    server.stop();
    REQUIRE_NOTHROW(thread_server.join());
    REQUIRE_NOTHROW(thread_client.join());
    display("[TCP][client][server] Testing send/receive data");
  }
}

//
// Tests: UDP socket.
//
TEST_CASE("UDP socket tests", "[network][udp][socket][default]") {
  SECTION("Default UDP socket should have default data") {
    udp::socket socket1;
    udp::socket socket2;

    CHECK(socket1.fd() == -1);
    CHECK(socket1.host() == "");
    CHECK(socket1.port() == 0);
    CHECK(socket1.bound() == false);

    THEN("The two default UDP sockets should be equal") {
      CHECK(socket1 == socket2);
    }

    display("[UDP][socket] Testing default socket");
  }

  SECTION("Default UDP socket should have default behaviour") {
    udp::socket socket;
    std::vector<char> buffer;

    REQUIRE_THROWS_AS((void)socket.sendto(""), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.broadcast(""), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.recvfrom(buffer), std::logic_error);
    display("[UDP][socket] Testing default behavior");
  }

  SECTION("Should initialize a default datagram socket") {
    udp::socket socket1;
    udp::socket socket2;

    REQUIRE_NOTHROW(socket1.init_datagram_socket("127.0.0.1", 27017, false));

    THEN("fd should not be equal to -1, port shoudl be equal to 27017") {
      CHECK(socket1.fd() != -1);
      CHECK(socket1.host() == "127.0.0.1");
      CHECK(socket1.port() == 27017);
      CHECK(socket1.bound() == false);
      CHECK((socket1 == socket2) == false);
    }

    REQUIRE_NOTHROW(socket1.close());
    THEN("fd should be equal to -1") {
      CHECK(socket1.fd() == -1);
      CHECK(socket1 == socket2);
    }
  }

  SECTION(
      "Should initialize a default datagram socket with broadcasting mode "
      "enabled") {
    udp::socket socket1;
    udp::socket socket2;

    REQUIRE_NOTHROW(socket1.init_datagram_socket("127.0.0.1", 27017, true));

    THEN("fd should not be equal to -1, port shoudl be equal to 27017") {
      CHECK(socket1.fd() != -1);
      CHECK(socket1.host() == "127.0.0.1");
      CHECK(socket1.port() == 27017);
      CHECK(socket1.bound() == false);
      CHECK((socket1 == socket2) == false);
    }

    REQUIRE_NOTHROW(socket1.close());
    THEN("fd should be equal to -1") {
      CHECK(socket1.fd() == -1);
      CHECK(socket1 == socket2);
    }
  }

  SECTION("Should initialize and bind a default datagram socket") {
    udp::socket socket1;
    udp::socket socket2;

    REQUIRE_NOTHROW(socket1.bind("127.0.0.1", 27017));

    THEN("fd should not be equal to -1, port shoudl be equal to 27017") {
      CHECK(socket1.fd() != -1);
      CHECK(socket1.host() == "127.0.0.1");
      CHECK(socket1.port() == 27017);
      CHECK(socket1.bound());
      CHECK((socket1 == socket2) == false);
      display("[UDP][socket] Testing bind");
    }

    REQUIRE_NOTHROW(socket1.close());
    THEN("fd should be equal to -1") {
      CHECK(socket1.fd() == -1);
      CHECK(socket1 == socket2);
    }
  }
}

TEST_CASE("UDP socket tests sending/broadcasting/receiving data.",
          "[network][udp][socket][operations][server][client]") {
  SECTION("Should correctly send/receive data") {
    std::thread server([] {
      udp::socket socket;
      std::vector<char> buffer;

      buffer.reserve(internal::BUFFER_SIZE);
      REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
      REQUIRE(socket.bound());
      auto result = socket.recvfrom(buffer);
      REQUIRE(result == 13);
      REQUIRE_NOTHROW(socket.close());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread client([] {
      udp::socket socket;

      REQUIRE_NOTHROW(socket.init_datagram_socket("127.0.01", 27017, true));
      auto bytes_sent = socket.broadcast("Hello world!\n");
      REQUIRE(bytes_sent == 13);
      REQUIRE_NOTHROW(socket.close());
    });

    REQUIRE_NOTHROW(server.join());
    REQUIRE_NOTHROW(client.join());
    display("[UDP][socket] Testing send/receive data");
  }
}

//
// Tests: UDP server/client.
//
TEST_CASE("UDP server/client tests", "[network][udp][server][client]") {
  SECTION("Default UDP server should have default behaviour") {
    udp::server server;

    CHECK(!server.is_running());
    CHECK(server.get_socket().fd() == -1);
    CHECK(!server.get_socket().bound());
    REQUIRE_THROWS_AS(server.async_recvfrom(nullptr), std::logic_error);
    display("[UDP][server] Default behaviour");
  }

  SECTION("Default UDP client should have default behaviour") {
    udp::client client;

    CHECK(client.get_socket().fd() == -1);
    CHECK(!client.get_socket().bound());
    CHECK(!client.broadcast_mode_enabled());
    REQUIRE_NOTHROW(client.stop());
    display("[UDP][client] Default behaviour");
  }

  SECTION("Should correctly send and receive data") {
    udp::server server;
    udp::client client;

    std::thread thread_server([&server] {

      REQUIRE_NOTHROW(server.bind("127.0.0.1", 27017));

      server.async_recvfrom([](std::vector<char> data, int bytes_read) {
        REQUIRE(bytes_read == 13);
        REQUIRE((unsigned)std::strlen(data.data()) == 13);
      });

      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      REQUIRE_THROWS_AS(server.bind("127.0.0.1", 27017), std::logic_error);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread thread_client([&client] {

      REQUIRE_NOTHROW(client.init("127.0.0.1", 27017, true));

      client.async_broadcast("Hello world!\n", [](int bytes_sent) {
        REQUIRE(bytes_sent == 13);
      });

      client.stop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    server.stop();
    REQUIRE_NOTHROW(thread_server.join());
    REQUIRE_NOTHROW(thread_client.join());
    display("[UDP][client][server] Testing send/receive data");
  }
}
