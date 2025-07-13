#include <boost/asio.hpp>
#include <csignal>
#include <iostream>
#include "client/Client.hpp"

int main() {
    boost::asio::io_context io;

    auto client = std::make_shared<Client>(io, "127.0.0.1", 12345);
    client->connect();

    // Отправка сообщений из cin на сервер в виде опкода MESSAGE
    // если не нужно, просто закомментить
    std::thread([client]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            client->send_message(line);
        }
    }).detach();

    // Первый таймер — отправка первого пакета через 1000мс
    boost::asio::steady_timer timer1(io);
    timer1.expires_after(std::chrono::milliseconds(1000));
    timer1.async_wait([&client](const boost::system::error_code& ec) {
        if (!ec) {
            std::cout << "[Main] Sending 1 manual packet...\n";
            client->send_async_select_user_by_id(1);
        }
    });

    // Второй таймер — отправка второго пакета ещё через 2000мс (общая задержка)
    boost::asio::steady_timer timer2(io);
    timer2.expires_after(std::chrono::milliseconds(2000));
    timer2.async_wait([&client](const boost::system::error_code& ec) {
        if (!ec) {
            std::cout << "[Main] Sending 2 manual packet...\n";
            client->send_sync_select_user_by_id(1);
        }
    });

    // Второй таймер — отправка второго пакета ещё через 2000мс (общая задержка)
    boost::asio::steady_timer timer3(io);
    timer3.expires_after(std::chrono::milliseconds(3000));
    timer3.async_wait([&client](const boost::system::error_code& ec) {
        if (!ec) {
            std::cout << "[Main] Sending 3 manual packet...\n";
            client->send_async_update_user_name_by_id(1, "ALICE_TEST");
        }
    });

    // Настраиваем сигнал Ctrl+C
    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        std::cout << "\n[Main] Caught signal. Disconnecting client...\n";
        client->disconnect();

        // Останавливаем io_context после graceful shutdown
        io.stop();
    });

    io.run();

    std::cout << "[Main] Exiting.\n";
    return 0;
}
