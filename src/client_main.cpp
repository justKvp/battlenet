#include "../include/async_client.h"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

int main() {
    AsyncClient client("127.0.0.1", "1234");
    client.Connect();

    // Wait for connection
    int attempts = 0;
    while (!client.IsConnected() && attempts++ < 10) {
        std::this_thread::sleep_for(100ms);
    }

    if (client.IsConnected()) {
        client.SendInventoryMove(1, 5, 12345);
        std::this_thread::sleep_for(100ms);
        client.SendInventoryMove(2, 3, 67890);
    }

    std::this_thread::sleep_for(2s);
    return 0;
}