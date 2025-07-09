#include "Client.hpp"
#include <iostream>

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

Client::Client(boost::asio::io_context &io_context, const std::string &h, int p)
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

void Client::disconnect() {
    boost::asio::post(io, [this]() {
        boost::system::error_code ignored_ec;

        // Отменить таймеры
        heartbeat_timer.cancel(ignored_ec);
        reconnect_timer.cancel(ignored_ec);

        // Закрыть сокет
        if (socket.is_open()) {
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
            socket.close(ignored_ec);
        }

        connected = false;

        std::cout << "[Client] Disconnected from server\n";
    });
}

void Client::schedule_reconnect() {
    connected = false;
    socket.close();
    reconnect_timer.expires_after(3s);
    reconnect_timer.async_wait([this](const boost::system::error_code &) {
        std::cout << "[Client] Attempting reconnect...\n";
        connect();
    });
}

void Client::start_heartbeat() {
    heartbeat_timer.expires_after(5s);
    heartbeat_timer.async_wait([this](const boost::system::error_code &ec) {
        if (!ec && connected) {
            send_ping();
            start_heartbeat(); // schedule next
        }
    });
}

void Client::send_ping() {
    Packet ping;
    ping.opcode = Opcode::CMSG_PING;
    send_packet(ping);
}

void Client::send_message(const std::string &msg) {
    Packet p;
    p.opcode = Opcode::MESSAGE;
    p.buffer.write_string(msg);
    send_packet(p);
}

void Client::send_hello(const std::string &msg, const uint8_t &number, const float &value) {
    Packet p;
    p.opcode = Opcode::CMSG_HELLO;
    p.buffer.write_string(msg);
    p.buffer.write_uint8(number);
    p.buffer.write_float(value);
    send_packet(p);
}

void Client::send_packet(const Packet& packet) {
    std::vector<uint8_t> body = packet.serialize();

    ByteBuffer header;
    header.write_uint16(static_cast<uint16_t>(body.size()));

    std::vector<uint8_t> full_packet = header.data();
    full_packet.insert(full_packet.end(), body.begin(), body.end());

    if (!connected) {
        std::cout << "[Client] Not connected. Queuing packet.\n";
        outgoing_queue.push(full_packet);
        return;
    }

    outgoing_queue.push(full_packet);

    // 👉 ВСЕГДА запускаем отправку, если пишем только этот пакет
    if (outgoing_queue.size() == 1) {
        flush_queue();
    }
}

void Client::flush_queue() {
    if (!connected || outgoing_queue.empty())
        return;

    auto &front = outgoing_queue.front();
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

                                uint16_t body_size = ((*header)[0] << 8) | (*header)[1];
                                if (body_size == 0) {
                                    std::cerr << "[Client] Invalid body size (0), closing.\n";
                                    connected = false;
                                    schedule_reconnect();
                                    return;
                                }

                                auto body = std::make_shared<std::vector<uint8_t>>(body_size);

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
                                                            } catch (const std::exception& ex) {
                                                                std::cerr << "[Client] Failed to parse packet: " << ex.what() << "\n";
                                                            }

                                                            start_receive_loop(); // продолжить чтение
                                                        });
                            });
}


void Client::handle_packet(const Packet &p) {
    switch (p.opcode) {
        case Opcode::SMSG_PONG:
            std::cout << "[Client] Received SMSG_PONG\n";
            break;
        case Opcode::SMSG_HELLO_RES:
            std::cout << "[Client] Received SMSG_HELLO_RES\n";
            break;
        default:
            std::cout << "[Client] Unknown opcode: " << static_cast<int>(p.opcode) << "\n";
            break;
    }
}