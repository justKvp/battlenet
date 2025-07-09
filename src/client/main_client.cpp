#include "Client.hpp"

int main() {
    boost::asio::io_context io;
    Client client(io, "127.0.0.1", 5555);
    client.run();
    io.run();
    return 0;
}