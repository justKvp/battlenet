#include "server/Server.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        unsigned int network_threads = 4;
        boost::asio::thread_pool pool(network_threads);

        int port = 12345;

        // 1) Создаёшь сервер
        auto server = std::make_shared<Server>(pool.get_executor(), port);

        // 2) После создания — запускаешь accept
        server->start_accept();

        std::cout << "[Server] Running on port " << port << "\n";

        // Перехват SIGINT, SIGTERM
        boost::asio::signal_set signals(pool.get_executor(), SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int signal_number) {
            std::cout << "[Server] Signal " << signal_number << " received, shutting down...\n";
            server->stop();   // 🟢 Правильное закрытие!
            pool.stop();
        });

        pool.join(); // ждём все worker-потоки

        std::cout << "[Server] Shutting down gracefully.\n";

    } catch (const std::exception& e) {
        std::cerr << "[Server] Exception: " << e.what() << "\n";
    }

    return 0;
}