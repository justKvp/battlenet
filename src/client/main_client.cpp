#include <boost/asio.hpp>
#include <csignal>
#include <iostream>
#include "Client.hpp"

int main() {
    boost::asio::io_context io;

    auto client = std::make_shared<Client>(io, "127.0.0.1", 12345);
    client->connect();

    // Первый таймер — отправка первого пакета через 1000мс
    boost::asio::steady_timer timer1(io);
    timer1.expires_after(std::chrono::milliseconds(1000));
    timer1.async_wait([&client](const boost::system::error_code& ec) {
        if (!ec) {
            std::cout << "[Main] Sending first manual packet...\n";
            client->send_message("First manual message");
        }
    });

    // Второй таймер — отправка второго пакета ещё через 2000мс (общая задержка)
    boost::asio::steady_timer timer2(io);
    timer2.expires_after(std::chrono::milliseconds(2000));
    timer2.async_wait([&client](const boost::system::error_code& ec) {
        if (!ec) {
            std::cout << "[Main] Sending second manual packet...\n";
            client->send_hello("hello", 254, 10.5f);
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
