#include <boost/asio.hpp>
#include <iostream>
#include <csignal>

#include "Database.hpp"
#include "Logger.hpp"
#include "server/Server.hpp"

int main() {
    Logger::init_thread_pool();  // Инициализировать thread pool до первого лога!
    auto log = Logger::get();

    try {
        unsigned int network_threads = 4;
        int port = 12345;

        // Единый thread pool на всё приложение
        boost::asio::thread_pool pool(network_threads);

        // 🟢 Инициализируем базу данных (передаём тот же пул!)
        auto db = std::make_shared<Database>(pool, "host=185.185.59.232 port=58995 user=postgres password=postgres dbname=postgres", 4);

        // Сервер получает ссылку на pool и готовую БД
        auto server = std::make_shared<Server>(pool.get_executor(), db, port);

        server->start_accept();

        log->info("[Server] Running on port {}!", port);

        // Перехват SIGINT/SIGTERM
        boost::asio::signal_set signals(pool.get_executor(), SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int signal_number) {
            log->info("[Server] Signal {} received, shutting down...", signal_number);
            server->stop();
            pool.stop();
        });

        // Запускаем все worker-потоки
        pool.join();

        log->info("[Server] Gracefully shut down.");

    } catch (const std::exception& e) {
        log->error("[Server] Exception: {}", e.what());
    }

    return 0;
}
