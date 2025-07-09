#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include "Packet.hpp"

using boost::asio::ip::tcp;

class Client {
    boost::asio::io_context io;
    tcp::socket socket;
    std::string host;
    short port;
    bool connected = false;
    std::mutex mutex;
    std::queue<Packet> message_queue;

public:
    Client(const std::string& h, short p)
            : socket(io), host(h), port(p) {}

    void run() {
        std::thread heartbeat_thread([this]() { heartbeat_loop(); });
        std::thread input_thread([this]() { input_loop(); });

        while (true) {
            if (!connected) {
                connect();
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }

            flush_message_queue();  // Отправим всё накопленное
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        heartbeat_thread.join();
        input_thread.join();
    }

private:
    void connect() {
        try {
            tcp::resolver resolver(io);
            boost::asio::connect(socket, resolver.resolve(host, std::to_string(port)));
            connected = true;
            std::cout << "[Client] Connected to server\n";
        } catch (...) {
            connected = false;
            std::cerr << "[Client] Connection failed. Retrying...\n";
        }
    }

    void send_packet(const Packet& p) {
        if (!connected) return;

        try {
            auto data = p.serialize();
            boost::asio::write(socket, boost::asio::buffer(data));
        } catch (...) {
            connected = false;
            std::cerr << "[Client] Send error. Marking as disconnected.\n";
            socket.close();
            socket = tcp::socket(io);
        }
    }

    void heartbeat_loop() {
        while (true) {
            if (connected) {
                Packet ping;
                ping.opcode = Opcode::PING;
                send_packet(ping);
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    void input_loop() {
        std::string input;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, input)) break;
            if (input.empty()) continue;
            if (input == "exit") break;

            Packet p;
            p.opcode = Opcode::MESSAGE;
            p.buffer.write_string(input);

            std::lock_guard<std::mutex> lock(mutex);
            message_queue.push(p);
        }
    }

    void flush_message_queue() {
        std::lock_guard<std::mutex> lock(mutex);

        while (!message_queue.empty() && connected) {
            send_packet(message_queue.front());
            message_queue.pop();
        }
    }
};

int main() {
    Client client("127.0.0.1", 5555);
    client.run();
    return 0;
}