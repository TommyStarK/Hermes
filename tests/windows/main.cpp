#include "Hermes.hpp"

using namespace hermes;
using namespace hermes::network;

void tests_tcp_socket(void) {
	tcp::socket socket;

	assert(socket.get_fd() == -1);
	assert(socket.get_host() == "");
	assert(socket.get_port() == 0);
}

int __cdecl main(void) {
	tests_tcp_socket();
	std::cout << "Working :)\n";
	std::cout << "Working :)\n";
	std::cout << "Working :)\n";
	return 0;
}
