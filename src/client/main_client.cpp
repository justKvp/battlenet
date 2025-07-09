#include "Client.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;

        // Укажи нужный хост и порт
        std::string host = "127.0.0.1";
        int port = 12345;

        Client client(io_context, host, port);
        client.run();

        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "[Client] Exception: " << e.what() << "\n";
    }

    return 0;
}
