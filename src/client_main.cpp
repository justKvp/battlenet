#include "async_client.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        auto client = std::make_shared<AsyncClient>(io_context);
        client->connect(argv[1], argv[2]);

        // Отправка тестовых данных
        ByteBuffer auth_packet;
        auth_packet.write_string("test_user");
        client->send(Opcode::CLIENT_AUTH_INIT_REQ, auth_packet);

//        ByteBuffer data_packet;
//        data_packet.write<uint32_t>(42);
//        data_packet.write_string("Hello from client");
//        client->send(Opcode::CLIENT_HELLO_REQ, data_packet);

        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}