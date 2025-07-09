#include "Server.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;

        int port = 12345; // Порт, на котором будет слушать сервер
        auto server = std::make_shared<Server>(io_context, port);

        std::cout << "[Server] Running on port " << port << std::endl;

        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "[Server] Exception: " << e.what() << std::endl;
    }

    return 0;
}
