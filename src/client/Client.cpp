#include "Client.hpp"
#include <iostream>

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

Client::Client(boost::asio::io_context& io_context, const std::string& h, int p)
        : io(io_context), socket(io_context), host(h), port(p), connected(false),
          reconnect_timer(io_context), heartbeat_timer(io_context) {}

void Client::connect() {
    tcp::resolver resolver(io);
    auto endpoints = resolver.resolve(host, std::to_string(port));

    boost::asio::async_connect(socket, endpoints,
                               [this](boost::system::error_code ec, tcp::endpoint) {
                                   if (!ec) {
                                       connected = true;
                                       std::cout << "[Client] Connected to server\n";
                                       start_receive_loop();
                                       flush_queue();
                                       start_heartbeat();
                                   } else {
                                       std::cerr << "[Client] Connection failed: " << ec.message() << "\n";
                                       schedule_reconnect();
                                   }
                               });
}

void Client::schedule_reconnect() {
    connected = false;
    socket.close();
    reconnect_timer.expires_after(3s);
    reconnect_timer.async_wait([this](const boost::system::error_code&) {
        std::cout << "[Client] Attempting reconnect...\n";
        connect();
    });
}

void Client::start_heartbeat() {
    heartbeat_timer.expires_after(5s);
    heartbeat_timer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec && connected) {
            send_ping();
            start_heartbeat(); // schedule next
        }
    });
}

void Client::send_ping() {
    Packet ping;
    ping.opcode = Opcode::PING;
    send_packet(ping);
}

void Client::send_message(const std::string& msg) {
    Packet p;
    p.opcode = Opcode::MESSAGE;
    p.buffer.write_string(msg);
    send_packet(p);
}

void Client::send_packet(const Packet& packet) {
    std::vector<uint8_t> data = packet.serialize();

    if (!connected) {
        std::cout << "[Client] Not connected. Queuing packet.\n";
        outgoing_queue.push(data);
        return;
    }

    bool writing = !outgoing_queue.empty();
    outgoing_queue.push(data);

    if (!writing) {
        flush_queue();
    }
}

void Client::flush_queue() {
    if (!connected || outgoing_queue.empty())
        return;

    auto& front = outgoing_queue.front();
    boost::asio::async_write(socket, boost::asio::buffer(front),
                             [this](boost::system::error_code ec, std::size_t) {
                                 if (ec) {
                                     std::cerr << "[Client] Write failed: " << ec.message() << "\n";
                                     connected = false;
                                     schedule_reconnect();
                                     return;
                                 }
                                 outgoing_queue.pop();
                                 flush_queue(); // send next
                             });
}

void Client::start_receive_loop() {
    auto header = std::make_shared<std::vector<uint8_t>>(2);

    boost::asio::async_read(socket, boost::asio::buffer(*header),
                            [this, header](boost::system::error_code ec, std::size_t) {
                                if (ec) {
                                    std::cerr << "[Client] Read header failed: " << ec.message() << "\n";
                                    connected = false;
                                    schedule_reconnect();
                                    return;
                                }

                                uint16_t size = ((*header)[0] << 8) | (*header)[1];
                                auto body = std::make_shared<std::vector<uint8_t>>(size);

                                boost::asio::async_read(socket, boost::asio::buffer(*body),
                                                        [this, body](boost::system::error_code ec2, std::size_t) {
                                                            if (ec2) {
                                                                std::cerr << "[Client] Read body failed: " << ec2.message() << "\n";
                                                                connected = false;
                                                                schedule_reconnect();
                                                                return;
                                                            }

                                                            try {
                                                                Packet p = Packet::deserialize(*body);
                                                                handle_packet(p);
                                                            } catch (...) {
                                                                std::cerr << "[Client] Failed to parse packet\n";
                                                            }

                                                            start_receive_loop(); // loop again
                                                        });
                            });
}

void Client::handle_packet(const Packet& p) {
    switch (p.opcode) {
        case Opcode::PONG:
            std::cout << "[Client] Received PONG\n";
            break;
        default:
            std::cout << "[Client] Unknown opcode: " << static_cast<int>(p.opcode) << "\n";
            break;
    }
}

void Client::run() {
    connect();

    // Read user input in a separate thread
    std::thread([this]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            send_message(line);
        }
    }).detach();
}
