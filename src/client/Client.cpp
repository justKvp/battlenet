#include "Client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using boost::asio::ip::tcp;

Client::Client(boost::asio::io_context& io_, const std::string& host_, int port_)
        : io(io_), socket(io_), host(host_), port(port_) {}

void Client::connect() {
    try {
        tcp::resolver resolver(io);
        boost::asio::connect(socket, resolver.resolve(host, std::to_string(port)));
        connected = true;
        std::cout << "[Client] Connected to server\n";

        start_receive_loop();
        flush_queue();
    } catch (...) {
        connected = false;
        std::cerr << "[Client] Connection failed. Retrying...\n";
        socket = tcp::socket(io);
    }
}

void Client::reconnect() {
    while (!connected) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        connect();
    }
}

void Client::send_ping() {
    Packet p;
    p.opcode = Opcode::PING;
    queue_packet(p);
}

void Client::send_message(const std::string& msg) {
    Packet p;
    p.opcode = Opcode::MESSAGE;
    p.buffer.write_string(msg);
    queue_packet(p);
}

void Client::queue_packet(const Packet& p) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    outgoing_queue.push(p.serialize());
    if (connected) flush_queue();
}

void Client::flush_queue() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!outgoing_queue.empty()) {
        auto data = outgoing_queue.front();
        outgoing_queue.pop();
        boost::asio::async_write(socket, boost::asio::buffer(data),
                                 [](boost::system::error_code, std::size_t) {});
    }
}

void Client::start_receive_loop() {
    auto self = this;
    auto header_buf = std::make_shared<std::vector<uint8_t>>(2);

    boost::asio::async_read(socket, boost::asio::buffer(*header_buf),
                            [this, self, header_buf](boost::system::error_code ec, std::size_t) {
                                if (ec) {
                                    connected = false;
                                    std::cerr << "[Client] Disconnected during read\n";
                                    reconnect();
                                    return;
                                }

                                uint16_t size = ((*header_buf)[0] << 8) | (*header_buf)[1];
                                auto body_buf = std::make_shared<std::vector<uint8_t>>(size);

                                boost::asio::async_read(socket, boost::asio::buffer(*body_buf),
                                                        [this, self, body_buf](boost::system::error_code ec2, std::size_t) {
                                                            if (ec2) {
                                                                connected = false;
                                                                std::cerr << "[Client] Disconnected during read body\n";
                                                                reconnect();
                                                                return;
                                                            }

                                                            try {
                                                                Packet p = Packet::deserialize(*body_buf);
                                                                handle_packet(p);
                                                            } catch (...) {
                                                                std::cerr << "[Client] Failed to parse packet\n";
                                                            }

                                                            start_receive_loop();
                                                        });
                            });
}

void Client::handle_packet(const Packet& p) {
    switch (p.opcode) {
        case Opcode::PONG:
            std::cout << "[Client] Received PONG\n";
            break;
        default:
            std::cout << "[Client] Received unknown opcode: " << static_cast<int>(p.opcode) << "\n";
            break;
    }
}

void Client::run() {
    std::thread([this]() {
        while (true) {
            if (!connected) connect();
            send_ping();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();

    std::string input;
    while (std::getline(std::cin, input)) {
        send_message(input);
    }
}