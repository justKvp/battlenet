#include <iostream>
#include "Server.hpp"

int main() {
    try {
        boost::asio::io_context io;
        Server server(io, 5555);
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }
    return 0;
}
