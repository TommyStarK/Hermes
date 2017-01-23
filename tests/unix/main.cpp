#define CATCH_CONFIG_MAIN
#include "Hermes.hpp"
#include "catch.hpp"

using namespace hermes;
using namespace hermes::network;

//
// Tests: Event.
//
TEST_CASE("Event model tests", "[model][event]") {
  SECTION("Should have default value for an empty event model") {
    event event;

    CHECK(!event.unwatch_);
    CHECK(!event.on_send_.running);
    CHECK(!event.on_send_.callback);
    CHECK(!event.on_receive_.running);
    CHECK(!event.on_receive_.callback);
    CHECK(event.has() == false);
    CHECK(event.pollfd_.fd == -1);
    CHECK(event.pollfd_.events == 0);
    CHECK(event.pollfd_.revents == 0);
  }

  SECTION("Should correctly set the specified event to monitor") {
    event event;
    auto send_callback = []() {};
    auto receive_callback = []() {};

    event.on_send_.callback = send_callback;
    event.update(4);

    THEN("fd should be equal to 4 and event flag set to POLLOUT") {
      CHECK(event.pollfd_.fd == 4);
      CHECK(event.pollfd_.events == POLLOUT);
      CHECK(event.has());
    }

    event.reset_poll_struct();
    event.on_send_.callback = nullptr;

    THEN("pollfd struct: fd should be equal to -1 and event flag set to 0") {
      CHECK(event.pollfd_.fd == -1);
      CHECK(event.pollfd_.events == 0);
      CHECK(event.has() == false);
    }

    event.on_receive_.callback = receive_callback;
    event.update(42);

    THEN("pollfd struct: fd should be equal to 42 and event set to POLLIN") {
      CHECK(event.pollfd_.fd == 42);
      CHECK(event.pollfd_.events == POLLIN);
      CHECK(event.has());
    }

    event.reset_poll_struct();
    event.on_receive_.callback = nullptr;
  }
}

//
// Tests: Polling model.
//
TEST_CASE("Poll model tests", "[model][poll]") {
  SECTION("Should create an empty polling model") {
    REQUIRE(poller_g == nullptr);

    std::shared_ptr<poller> poller;
    poller = get_poller();

    THEN("Should create an instance for the poller singleton") {
      REQUIRE(poller != nullptr);
      REQUIRE(poller_g != nullptr);
    }

    tcp::socket socket;
    REQUIRE(poller->has<tcp::socket>(socket) == false);

    poller->add<tcp::socket>(socket);
    REQUIRE(poller->has<tcp::socket>(socket) == true);

    poller->remove<tcp::socket>(socket);
    REQUIRE(poller->has<tcp::socket>(socket) == false);
  }
}

//
// Tests: Workers.
//
TEST_CASE("Workers tests", "[tools][workers]") {
  SECTION("Should work from their construction") {
    tools::workers workers;

    CHECK(workers.are_working());

    workers.stop();
    THEN("Should stop working") { CHECK(!workers.are_working()); }
  }

  SECTION("Should process the enqueued jobs correctly") {
    tools::workers workers;

    workers.enqueue_job(nullptr);
    workers.enqueue_job([]() { std::cout << "processing: job 1\n"; });
    workers.enqueue_job([]() { std::cout << "processing: job 2\n"; });
    workers.enqueue_job([]() { std::cout << "processing: job 3\n"; });
    workers.enqueue_job([]() { std::cout << "processing: job 4\n"; });
    workers.enqueue_job([]() { std::cout << "processing: job 5\n"; });

    workers.stop();
    THEN("Should stop working") { CHECK(!workers.are_working()); }
  }
}

//
// Tests: TCP socket.
//
TEST_CASE("TCP socket tests", "[network][tcp][socket][default]") {
  SECTION("Default TCP socket should have default data") {
    tcp::socket socket1;
    tcp::socket socket2;

    CHECK(socket1.get_fd() == -1);
    CHECK(socket1.get_host() == "");
    CHECK(socket1.get_port() == 0);
    CHECK(socket1.has_a_name_assigned() == false);

    THEN("The two default TCP sockets should be equal") {
      CHECK(socket1 == socket2);
    }
  }

  SECTION("Default TCP socket should have default behaviour") {
    tcp::socket socket;

    REQUIRE_THROWS_AS(socket.listen(), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.accept(), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.send(""), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.receive(1024), std::logic_error);
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

    REQUIRE(socket1.get_fd() != -1);
    CHECK(socket1.get_host() == "127.0.0.1");
    CHECK(socket1.get_port() == 27017);
    CHECK(socket1.has_a_name_assigned());
    CHECK(!(socket1 == socket2));

    REQUIRE_NOTHROW(socket1.close());
    THEN("Should be equal to socket2") { CHECK(socket1 == socket2); }
  }

  SECTION("Should mark the socket as passive") {
    tcp::socket socket;

    REQUIRE_THROWS_AS(socket.listen(), std::logic_error);
    REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
    CHECK(socket.has_a_name_assigned());
    REQUIRE_NOTHROW(socket.listen(50));

    THEN("Trying to connect a socket marked as passive should throw an error") {
      REQUIRE_THROWS_AS(socket.connect("127.0.0.1", 27017), std::logic_error);
    }

    REQUIRE_NOTHROW(socket.close());

    THEN("Should reset the socket") {
      CHECK(socket.get_fd() == -1);
      CHECK(!socket.has_a_name_assigned());
    }
  }

  SECTION("Should accept a new connection") {
    std::thread server([] {
      tcp::socket socket;

      REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
      REQUIRE_NOTHROW(socket.listen());
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
  }
}

TEST_CASE("TCP socket tests client operations",
          "[network][tcp][socket][operations][client]") {
  SECTION("Should correctly send/receive data") {
    std::thread server([] {
      tcp::socket socket;

      REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
      REQUIRE_NOTHROW(socket.listen());
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
  }
}

//
// Tests: TCP server/client.
//
TEST_CASE("TCP server/client tests", "[network][tcp][server][client]") {
  SECTION("Default TCP server should have default behaviour") {
    tcp::server server;

    CHECK(!server.is_running());
    CHECK(server.get_socket().get_fd() == -1);
    CHECK(!server.get_socket().has_a_name_assigned());
    CHECK_THROWS_AS(server.run("127.0.0.1", 27017), std::invalid_argument);
  }

  SECTION("Default TCP client should have default behaviour") {
    tcp::client client;

    CHECK(!client.is_connected());
    CHECK(client.get_socket().get_fd() == -1);
    CHECK(!client.get_socket().has_a_name_assigned());
    CHECK_THROWS_AS(client.async_send("", nullptr), std::logic_error);
    CHECK_THROWS_AS(client.async_receive(1024, nullptr), std::logic_error);
  }

  SECTION("Should correctly send and receive data") {
    tcp::server server;
    tcp::client client;

    std::thread thread_server([&server] {

      server.on_connection([](const std::shared_ptr<tcp::client> &client) {
        client->async_receive(1024, [](bool success, std::vector<char> data) {
          if (success) std::cout << data.data();
        });
      });

      REQUIRE_NOTHROW(server.run("127.0.0.1", 27017));
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      REQUIRE_THROWS_AS(server.run("127.0.0.1", 27017), std::logic_error);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread thread_client([&client] {

      REQUIRE_NOTHROW(client.connect("127.0.0.1", 27017));

      client.async_send("Hello world!\n", [](bool success, std::size_t sent) {
        if (success) REQUIRE(sent == 13);
      });

      REQUIRE_THROWS_AS(client.connect("127.0.0.1", 27017), std::logic_error);
    });

    REQUIRE_NOTHROW(thread_server.join());
    REQUIRE_NOTHROW(thread_client.join());
  }
}

//
// Tests: UDP socket.
//
TEST_CASE("UDP socket tests", "[network][udp][socket][default]") {
  SECTION("Default UDP socket should have default data") {
    udp::socket socket1;
    udp::socket socket2;

    CHECK(socket1.get_fd() == -1);
    CHECK(socket1.get_host() == "");
    CHECK(socket1.get_port() == 0);
    CHECK(socket1.has_a_name_assigned() == false);

    THEN("The two default UDP sockets should be equal") {
      CHECK(socket1 == socket2);
    }
  }

  SECTION("Default UDP socket should have default behaviour") {
    udp::socket socket;
    std::vector<char> buffer;

    REQUIRE_THROWS_AS((void)socket.sendto(""), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.broadcast(""), std::logic_error);
    REQUIRE_THROWS_AS((void)socket.recvfrom(buffer), std::logic_error);
  }

  SECTION("Should initialize a default datagram socket") {
    udp::socket socket1;
    udp::socket socket2;

    REQUIRE_NOTHROW(socket1.init_datagram_socket("127.0.0.1", 27017, false));

    THEN("fd should not be equal to -1, port shoudl be equal to 27017") {
      CHECK(socket1.get_fd() != -1);
      CHECK(socket1.get_host() == "127.0.0.1");
      CHECK(socket1.get_port() == 27017);
      CHECK(socket1.has_a_name_assigned() == false);
      CHECK((socket1 == socket2) == false);
    }

    REQUIRE_NOTHROW(socket1.close());
    THEN("fd should be equal to -1") {
      CHECK(socket1.get_fd() == -1);
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
      CHECK(socket1.get_fd() != -1);
      CHECK(socket1.get_host() == "127.0.0.1");
      CHECK(socket1.get_port() == 27017);
      CHECK(socket1.has_a_name_assigned() == false);
      CHECK((socket1 == socket2) == false);
    }

    REQUIRE_NOTHROW(socket1.close());
    THEN("fd should be equal to -1") {
      CHECK(socket1.get_fd() == -1);
      CHECK(socket1 == socket2);
    }
  }

  SECTION("Should initialize and bind a default datagram socket") {
    udp::socket socket1;
    udp::socket socket2;

    REQUIRE_NOTHROW(socket1.bind("127.0.0.1", 27017));

    THEN("fd should not be equal to -1, port shoudl be equal to 27017") {
      CHECK(socket1.get_fd() != -1);
      CHECK(socket1.get_host() == "127.0.0.1");
      CHECK(socket1.get_port() == 27017);
      CHECK(socket1.has_a_name_assigned());
      CHECK((socket1 == socket2) == false);
    }

    REQUIRE_NOTHROW(socket1.close());
    THEN("fd should be equal to -1") {
      CHECK(socket1.get_fd() == -1);
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

      buffer.reserve(tools::BUFFER_SIZE);
      REQUIRE_NOTHROW(socket.bind("127.0.0.1", 27017));
      REQUIRE(socket.has_a_name_assigned());
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
  }
}

//
// Tests: UDP server/client.
//
TEST_CASE("UDP server/client tests", "[network][udp][server][client]") {
  SECTION("Default UDP server should have default behaviour") {
    udp::server server;

    CHECK(!server.is_running());
    CHECK(server.get_socket().get_fd() == -1);
    CHECK(!server.get_socket().has_a_name_assigned());
    REQUIRE_THROWS_AS(server.async_recvfrom(nullptr), std::logic_error);
  }

  SECTION("Default UDP client should have default behaviour") {
    udp::client client;

    CHECK(client.get_socket().get_fd() == -1);
    CHECK(!client.get_socket().has_a_name_assigned());
    CHECK(!client.broadcast_mode_enabled());
  }

  SECTION("Should correctly send and receive data") {
    udp::server server;
    udp::client client;

    std::thread thread_server([&server] {

      REQUIRE_NOTHROW(server.bind("127.0.0.1", 27017));

      server.async_recvfrom([](std::vector<char> data, int bytes_sent) {
        if (bytes_sent) {
          REQUIRE(bytes_sent == 13);
          std::cout << data.data();
        }
      });

      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      REQUIRE_THROWS_AS(server.bind("127.0.0.1", 27017), std::logic_error);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::thread thread_client([&client] {

      REQUIRE_NOTHROW(client.init("127.0.0.1", 27017, true));

      client.async_broadcast("Hello world!\n", [](int bytes_sent) {
        if (bytes_sent) {
          REQUIRE(bytes_sent == 13);
        }
      });

    });

    REQUIRE_NOTHROW(thread_server.join());
    REQUIRE_NOTHROW(thread_client.join());
  }
}
