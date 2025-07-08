#include "async_client.h"
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<bool> client_running(true);

int main() {
    try {
        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("localhost", "1234");

        auto client = std::make_shared<AsyncClient>(io_context, endpoints);

        // Установка обработчиков
        client->SetResponseHandler([](bool success) {
            CLIENT_LOG("Operation result: " << (success ? "SUCCESS" : "FAILED"));
            client_running = false;
        });

        client->SetErrorHandler([](const std::string& error_msg) {
            CLIENT_LOG("Error: " << error_msg);
            client_running = false;
        });

        // Запуск с callback'ом при успешном соединении
        client->Start([client]() {
            CLIENT_LOG("Sending inventory move request...");
            client->SendInventoryMove(1, 5, 12345);
        });

        std::thread thread([&io_context](){
            io_context.run();
        });

        // Ожидание завершения
        while (client_running) {
            std::this_thread::sleep_for(100ms);
        }

        client->Disconnect();
        io_context.stop();
        thread.join();

        CLIENT_LOG("Client shutdown complete");
    } catch (const std::exception& e) {
        CLIENT_LOG("Fatal error: " << e.what());
        return 1;
    }
    return 0;
}