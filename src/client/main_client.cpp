#include "Client.hpp"
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context io_context;
    Client client(io_context, "127.0.0.1", 12345);
    client.run();
    io_context.run();
    return 0;
}