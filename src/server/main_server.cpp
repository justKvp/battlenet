#include "Server.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;

        int port = 12345; // Порт, на котором будет слушать сервер
        auto server = std::make_shared<Server>(io_context, port);

        std::cout << "[Server] Running on port " << port << std::endl;

        // Подписываемся на SIGINT и SIGTERM
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int signal_number) {
            std::cout << "[Server] Signal " << signal_number << " received, shutting down...\n";
            io_context.stop();
        });

        io_context.run();

        std::cout << "[Server] Shutting down gracefully.\n";
    } catch (const std::exception& e) {
        std::cerr << "[Server] Exception: " << e.what() << std::endl;
    }

    return 0;
}
