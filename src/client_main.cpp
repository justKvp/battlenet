#include "async_client.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: client <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        // Создаем и настраиваем клиента
        auto client = std::make_shared<AsyncClient>(io_context);
        client->connect(argv[1], argv[2]);

        // Пример отправки аутентификационного запроса
        ByteBuffer auth_packet;
        auth_packet.write_string("test_user");
        client->send(Opcode::AUTH_REQUEST, auth_packet);

        // Пример отправки данных
        ByteBuffer data_packet;
        data_packet.write<uint32_t>(42);
        data_packet.write_string("Sample data");
        client->send(Opcode::DATA_REQUEST, data_packet);

        // Запускаем обработку событий
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Client exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}