#include "ClientSession.hpp"
#include "Server.hpp"
#include <iostream>

using boost::asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket_, Server &server_)
        : socket(std::move(socket_)), server(server_) {}

void ClientSession::start() {
    read_header();
}

void ClientSession::read_header() {
    auto self = shared_from_this();
    read_buffer.resize(2); // 2 байта заголовка

    boost::asio::async_read(socket, boost::asio::buffer(read_buffer),
                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                if (ec) {
                                    std::cerr << "[Server] Header read failed: " << ec.message() << "\n";
                                    server.remove_session(self);
                                    return;
                                }

                                uint16_t size = (read_buffer[0] << 8) | read_buffer[1];
                                read_body(size);
                            });
}

void ClientSession::read_body(std::size_t length) {
    auto self = shared_from_this();
    read_buffer.resize(length);

    boost::asio::async_read(socket, boost::asio::buffer(read_buffer),
                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                if (ec) {
                                    std::cerr << "[Server] Body read failed: " << ec.message() << "\n";
                                    server.remove_session(self);
                                    return;
                                }

                                try {
                                    Packet packet = Packet::deserialize(read_buffer);
                                    handle_packet(packet);
                                } catch (...) {
                                    std::cerr << "[Server] Failed to parse packet\n";
                                }

                                read_header(); // Continue reading next packet
                            });
}

void ClientSession::handle_packet(Packet &packet) {
    switch (packet.opcode) {
        case Opcode::PING: {
            std::cout << "[Server] Received PING\n";
            Packet pong;
            pong.opcode = Opcode::PONG;
            send_packet(pong);
            break;
        }
        case Opcode::MESSAGE: {
            std::string msg = packet.buffer.read_string();
            std::cout << "[Server] Received message: " << msg << "\n";
            break;
        }
        default:
            std::cout << "[Server] Unknown opcode: " << static_cast<int>(packet.opcode) << "\n";
            break;
    }
}

void ClientSession::send_packet(const Packet &packet) {
    auto data = std::make_shared<std::vector<uint8_t>>(packet.serialize());

    auto self = shared_from_this();
    boost::asio::async_write(socket, boost::asio::buffer(*data),
                             [this, self, data](boost::system::error_code ec, std::size_t /*length*/) {
                                 if (ec) {
                                     std::cerr << "[Server] Failed to send packet: " << ec.message() << "\n";
                                     server.remove_session(self);
                                 }
                             });
}
