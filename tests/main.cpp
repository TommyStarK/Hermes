#define CATCH_CONFIG_MAIN
#include "Hermes.hpp"
#include "catch.hpp"

using namespace hermes;
using namespace hermes::network;

//
// Workers tests section
//
SCENARIO("testing workers (Thread pool)") {
  WHEN("using default constructor") {
    hermes::tools::workers workers;

    REQUIRE(workers.are_working());
    workers.stop();
    REQUIRE(!workers.are_working());
  }

  WHEN("giving 4 jobs to process to a thread pool") {
    hermes::tools::workers workers;

    REQUIRE_NOTHROW(workers.enqueue_job([]() {}));
    REQUIRE_NOTHROW(workers.enqueue_job([]() {}));
    REQUIRE_NOTHROW(workers.enqueue_job([]() {}));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE_NOTHROW(workers.enqueue_job([&]() { workers.stop(); }));
  }
}

//
// TCP socket tests sections
//
SCENARIO("testing TCP socket default constructor") {
  tcp::socket socket;
  tcp::socket socket2;

  REQUIRE(socket.get_fd() == -1);
  REQUIRE(socket.get_host() == "");
  REQUIRE(socket.get_port() == 0);
  REQUIRE(socket == socket2);
}

SCENARIO("testing TCP socket: server operations") {
  GIVEN("default TCP sockets") {
    tcp::socket default_socket;
    tcp::socket socket_for_test;

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
        auto client = std::make_shared<tcp::socket>(socket_for_test.accept());
        REQUIRE((socket_for_test == *client) == false);
        REQUIRE(client->get_fd() != socket_for_test.get_fd());
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

SCENARIO("testing TCP socket: client operations") {
  GIVEN("default TCP sockets") {
    tcp::socket default_socket;
    tcp::socket socket_for_test;

    WHEN("connecting to the given endpoint") {
      std::thread server([&default_socket]() {
        REQUIRE_NOTHROW(default_socket.bind("127.0.0.1", 27017));
        REQUIRE_NOTHROW(default_socket.listen());
        auto client = std::make_shared<tcp::socket>(default_socket.accept());
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

    WHEN("sending/receiving data") {
      std::thread server([&default_socket]() {
        REQUIRE_NOTHROW(default_socket.bind("127.0.0.1", 27017));
        REQUIRE_NOTHROW(default_socket.listen());
        auto client = std::make_shared<tcp::socket>(default_socket.accept());
        std::string rcv(client->receive().data());
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

//
// TCP client tests section
//
SCENARIO("testing TCP client") {
  WHEN("constructing a default TCP client") {
    tools::TIMEOUT = 0;

    tcp::client client;

    REQUIRE(!client.is_connected());
    set_poller(nullptr);
  }
}

//
// TCP server tests section
//
SCENARIO("testing TCP server") {
  WHEN("constructing a default TCP server") {
    tools::TIMEOUT = 0;

    tcp::server server;

    REQUIRE(!server.is_running());
    set_poller(nullptr);
  }
}

//
// UDP socket tests section
//

SCENARIO("testing UDP socket default constructor") {
  udp::socket socket;
  udp::socket socket2;

  REQUIRE(socket.get_fd() == -1);
  REQUIRE(socket.get_port() == 0);
  REQUIRE(socket == socket2);
}

SCENARIO("testing UDP socket: client operations") {
  GIVEN("default UDP sockets") {
    udp::socket socket1;
    udp::socket socket2;

    WHEN("Initializing a basic datagram socket with a given host/port") {
      REQUIRE_NOTHROW(socket1.init_datagram_socket("127.0.0.1", 27017, false));

      THEN("fd should not be equal to -1, port shoudl be equal to 27017") {
        REQUIRE(socket1.get_fd() != -1);
        REQUIRE(socket1.get_host() == "127.0.0.1");
        REQUIRE(socket1.get_port() == 27017);
        REQUIRE(socket1.is_socket_bound() == false);
        REQUIRE((socket1 == socket2) == false);
      }

      REQUIRE_NOTHROW(socket1.close());
      THEN("fd should be equal to -1") { REQUIRE(socket1.get_fd() == -1); }
    }
  }
}

SCENARIO("testing UDP socket: client operations - broadcasting") {
  GIVEN("default UDP sockets") {
    udp::socket socket1;
    udp::socket socket2;

    WHEN("Initializing a broadcasting socket with a given host/port") {
      REQUIRE_NOTHROW(socket1.init_datagram_socket("127.0.0.1", 27017, true));

      THEN("fd should not be equal to -1, port shoudl be equal to 27017") {
        REQUIRE(socket1.get_fd() != -1);
        REQUIRE(socket1.get_host() == "127.0.0.1");
        REQUIRE(socket1.get_port() == 27017);
        REQUIRE(socket1.is_socket_bound() == false);
        REQUIRE((socket1 == socket2) == false);
      }

      REQUIRE_NOTHROW(socket1.close());
      THEN("fd should be equal to -1") { REQUIRE(socket1.get_fd() == -1); }
    }
  }
}

SCENARIO("testing UDP socket: server operations") {
  GIVEN("default UDP sockets") {
    udp::socket socket1;
    udp::socket socket2;

    WHEN("creating and binding a datagram socket on a given host/port") {
      REQUIRE_NOTHROW(socket1.bind("", 27017));

      THEN(
          "fd should not be equal to -1, port should be equal to 27017, socket "
          "should now be bound to the given host/port") {
        REQUIRE(socket1.get_fd() != -1);
        REQUIRE(socket1.get_port() == 27017);
        REQUIRE(socket1.is_socket_bound());
        REQUIRE((socket1 == socket2) == false);
        REQUIRE_THROWS(socket1.bind("", 27017));
      }

      REQUIRE_NOTHROW(socket1.close());
      THEN("fd should be equal to -1") { REQUIRE(socket1.get_fd() == -1); }
    }
  }
}

SCENARIO("testing UDP socket: sending/receiving data") {
  GIVEN("default UDP sockets") {
    udp::socket socket1;
    udp::socket socket2;

    WHEN(
        "one thread working as server waiting for receive data from another "
        "thread working as client") {
      std::thread server([&socket1]() {
        std::vector<char> data;

        data.reserve(tools::BUFFER_SIZE);
        REQUIRE_NOTHROW(socket1.bind("127.0.0.1", 27017));
        REQUIRE(socket1.is_socket_bound());
        auto res = socket1.recvfrom(data);
        REQUIRE(res == 13);
        REQUIRE_NOTHROW(socket1.close());
        REQUIRE(socket1.get_fd() == -1);
      });

      std::this_thread::sleep_for(std::chrono::seconds(1));

      std::thread client([&socket2]() {
        REQUIRE_NOTHROW(
            socket2.init_datagram_socket("127.0.0.1", 27017, false));
        auto res = socket2.sendto("Hello world!\n");
        REQUIRE(res == 13);
        REQUIRE_NOTHROW(socket2.close());
        REQUIRE(socket2.get_fd() == -1);
      });

      REQUIRE_NOTHROW(server.join());
      REQUIRE_NOTHROW(client.join());
    }
  }
}

//
// UDP client tests section
//
SCENARIO("testing UDP client") {
  WHEN("constructing a default UDP client") {
    tools::TIMEOUT = 0;

    udp::client client;

    REQUIRE(!client.broadcast_mode_enabled());
    set_poller(nullptr);
  }
}

//
// UDP server tests section
//
SCENARIO("testing UDP server") {
  WHEN("constructing a default UDP server") {
    tools::TIMEOUT = 0;

    udp::server server;

    REQUIRE(!server.is_running());
    set_poller(nullptr);
  }
}

//
// Event model tests sections
//

SCENARIO("testing event model features") {
  GIVEN("empty model event") {
    event event;

    WHEN("testing empty event") {
      REQUIRE(!event.unwatch_);
      REQUIRE(!event.on_send_.running);
      REQUIRE(!event.on_send_.callback);
      REQUIRE(!event.on_receive_.running);
      REQUIRE(!event.on_receive_.callback);
      REQUIRE(event.has() == false);
      REQUIRE(event.pollfd_.fd == -1);
      REQUIRE(event.pollfd_.events == 0);
      REQUIRE(event.pollfd_.revents == 0);
    }

    WHEN("setting a specific event to monitor for this event") {
      auto send_callback = []() {};
      auto receive_callback = []() {};

      event.on_send_.callback = send_callback;
      event.update(4);

      THEN("fd should be equal to 4 and event flag set to POLLOUT") {
        REQUIRE(event.pollfd_.fd == 4);
        REQUIRE(event.pollfd_.events == POLLOUT);
        REQUIRE(event.has());
      }

      event.reset_poll_struct();
      event.on_send_.callback = nullptr;

      THEN("pollfd struct: fd should be equal to -1 and event flag set to 0") {
        REQUIRE(event.pollfd_.fd == -1);
        REQUIRE(event.pollfd_.events == 0);
        REQUIRE(event.has() == false);
      }

      event.on_receive_.callback = receive_callback;
      event.update(42);

      THEN("pollfd struct: fd should be equal to 42 and event set to POLLIN") {
        REQUIRE(event.pollfd_.fd == 42);
        REQUIRE(event.pollfd_.events == POLLIN);
        REQUIRE(event.has());
      }

      event.reset_poll_struct();
      event.on_receive_.callback = nullptr;
    }
  }
}

//
// Polling model tests section
//
SCENARIO("testing polling model features") {
  WHEN("creating an empty polling model") {
    tools::TIMEOUT = 0;

    REQUIRE(poller_g == nullptr);

    std::shared_ptr<poller> poller;
    poller = get_poller();

    THEN("creating an instance for the poller singleton") {
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
