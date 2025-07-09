#include "ClientSession.hpp"
#include <iostream>

using boost::asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket)
        : socket_(std::move(socket)) {}

void ClientSession::start() {
    read_header();
}

void ClientSession::read_header() {
    header_buffer_.resize(2);
    auto self = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(header_buffer_),
                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                if (ec) {
                                    if (ec == boost::asio::error::eof) {
                                        std::cout << "[Server] Client closed the connection\n";
                                    } else {
                                        std::cerr << "[Server] Header read failed: " << ec.message() << "\n";
                                    }
                                    socket_.close();
                                    return;
                                }

                                uint16_t size = (header_buffer_[0] << 8) | header_buffer_[1];
                                read_body(size);
                            });
}

void ClientSession::read_body(std::size_t size) {
    body_buffer_.resize(size);
    auto self = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(body_buffer_),
                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                if (ec) {
                                    if (ec == boost::asio::error::eof) {
                                        std::cout << "[Server] Client closed the connection\n";
                                    } else {
                                        std::cerr << "[Server] Body read failed: " << ec.message() << "\n";
                                    }
                                    socket_.close();
                                    return;
                                }

                                try {
                                    Packet packet = Packet::deserialize(body_buffer_);
                                    handle_packet(packet);
                                } catch (...) {
                                    std::cerr << "[Server] Failed to deserialize packet\n";
                                }

                                read_header();
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
            std::cout << "[Server] Received Message: " << msg << "\n";
            break;
        }
        default:
            std::cout << "[Server] Unknown opcode: " << static_cast<int>(packet.opcode) << "\n";
            break;
    }
}

void ClientSession::send_packet(const Packet &packet) {
    std::vector<uint8_t> data = packet.serialize();
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(data),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                 if (ec) {
                                     std::cerr << "[Server] Write failed: " << ec.message() << "\n";
                                     socket_.close();
                                 }
                             });
}
