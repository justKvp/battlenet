#include "ClientSession.hpp"
#include "Server.hpp"
#include <iostream>

using boost::asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket, std::shared_ptr<Server> server)
        : socket_(std::move(socket)), server_(std::move(server)) {}

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
                                    close();
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
                                    close();
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

void ClientSession::handle_packet(Packet& packet) {
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
            std::cout << "[Server] Message: " << msg << "\n";
            break;
        }
        default:
            std::cout << "[Server] Unknown opcode: " << static_cast<int>(packet.opcode) << "\n";
            break;
    }
}

void ClientSession::send_packet(const Packet& packet) {
    std::vector<uint8_t> data = packet.serialize();
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(data),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                 if (ec) {
                                     std::cerr << "[Server] Write failed: " << ec.message() << "\n";
                                     close();
                                 }
                             });
}

void ClientSession::close() {
    if (socket_.is_open()) {
        boost::system::error_code ignored_ec;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket_.close(ignored_ec);
    }
    if (server_) {
        server_->remove_session(shared_from_this());
    }
}
