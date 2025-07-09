#include "../include/async_server.h"
#include <iostream>

void HandleInventoryMove(ByteBuffer& buffer) {
    try {
        int from = buffer.ReadInt32();
        int to = buffer.ReadInt32();
        int item = buffer.ReadInt32();

        std::cout << "[SERVER] Inventory move: "
                  << from << " -> " << to << " (item " << item << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Invalid inventory move packet: " << e.what() << "\n";
    }
}

int main() {
    try {
        boost::asio::io_context io_context;
        AsyncServer server(io_context, 1234);

        // Запускаем обработку событий в отдельном потоке
        std::thread server_thread([&io_context](){
            io_context.run();
        });

        server_thread.join();
    } catch (const std::exception& e) {
        std::cerr << "Server exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}