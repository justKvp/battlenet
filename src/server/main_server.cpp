#include "Server.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;
        Server server(io_context, 12345);
        server.start_accept();
        io_context.run();
    } catch (const std::exception &e) {
        std::cerr << "[Server] Exception: " << e.what() << std::endl;
    }

    return 0;
}
