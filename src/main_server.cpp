#include "server/Server.hpp"
#include "Database.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <csignal>

int main() {
    try {
        unsigned int network_threads = 4;
        int port = 12345;

        // Единый thread pool на всё приложение
        boost::asio::thread_pool pool(network_threads);

        // 🟢 Инициализируем базу данных (передаём тот же пул!)
        auto db = std::make_shared<Database>(pool, "host=localhost user=postgres password=123 dbname=test");

        // Сервер получает ссылку на pool и готовую БД
        auto server = std::make_shared<Server>(pool.get_executor(), db, port);

        server->start_accept();

        std::cout << "[Server] Running on port " << port << "\n";

        // Перехват SIGINT/SIGTERM
        boost::asio::signal_set signals(pool.get_executor(), SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int signal_number) {
            std::cout << "[Server] Signal " << signal_number << " received, shutting down...\n";
            server->stop();
            pool.stop();
        });

        // Запускаем все worker-потоки
        pool.join();

        std::cout << "[Server] Gracefully shut down.\n";

    } catch (const std::exception& e) {
        std::cerr << "[Server] Exception: " << e.what() << "\n";
    }

    return 0;
}
