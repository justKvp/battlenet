#include "async_server.hpp"
#include <boost/asio.hpp>
#include <csignal>
#include <memory>

std::shared_ptr<AsyncServer> server;

void handle_signal(int) {
    if (server) {
        std::cout << "\nShutting down server...\n";
        server->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([](auto, int sig) { handle_signal(sig); });

        server = std::make_shared<AsyncServer>(io_context, std::atoi(argv[1]));
        server->start();

        std::cout << "Server running on port " << argv[1]
                  << " (Ctrl+C to stop)\n";
        io_context.run();

        std::cout << "Server stopped\n";
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}