#include "async_server.h"
#include <iostream>
#include <boost/asio.hpp>

int main() {
    try {
        boost::asio::io_context io_context;
        AsyncServer server(io_context, 1234);

        // Таймер для проверки мертвых соединений
        boost::asio::steady_timer timer(io_context);
        std::function<void()> check_connections = [&]() {
            server.check_dead_connections();
            timer.expires_after(std::chrono::seconds(10));
            timer.async_wait([&](auto) { check_connections(); });
        };
        check_connections();

        // Обработчик перемещения предметов
        server.set_move_handler([](uint8_t bag, uint8_t slot, uint32_t itemId) {
            // Логика обработки перемещения
        });

        // Запускаем обработку событий в отдельном потоке
        std::thread thread([&io_context](){
            io_context.run();
        });

        SERVER_LOG("Server started on port 1234. Press Enter to stop...");
        std::cin.get();

        // Остановка сервера
        server.stop();
        io_context.stop();
        thread.join();

        SERVER_LOG("Server stopped gracefully");
    } catch (const std::exception& e) {
        SERVER_LOG("Fatal error: " << e.what());
        return 1;
    }
    return 0;
}