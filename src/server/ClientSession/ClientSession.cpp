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

                                uint16_t body_size = (header_buffer_[0] << 8) | header_buffer_[1];
                                read_body(body_size);
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
                                } catch (const std::exception& ex) {
                                    std::cerr << "[Server] Failed to deserialize packet: " << ex.what() << "\n";
                                }

                                read_header();
                            });
}

void ClientSession::handle_packet(Packet& packet) {
    switch (packet.opcode) {
        case Opcode::MESSAGE: {
            std::string msg = packet.buffer.read_string();
            std::cout << "[Server] Message: " << msg << "\n";
            break;
        }
        case Opcode::CMSG_PING: {
            std::cout << "[Server] Received CMSG_PING\n";
            Packet pong;
            pong.opcode = Opcode::SMSG_PONG;
            send_packet(pong);
            break;
        }
        case Opcode::CMSG_HELLO: {
            std::string msg = packet.buffer.read_string();
            uint8_t number = packet.buffer.read_uint8();
            float value = packet.buffer.read_float();

            std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode) << "] SMSG_HELLO_RES : msg[" << msg << "] uint8_t[" << static_cast<int>(number) << "] float[" << value << "]\n";

            Packet resp;
            resp.opcode = Opcode::SMSG_HELLO_RES;
            send_packet(resp);
            break;
        }
        default:
            std::cout << "[Server] Unknown opcode: " << static_cast<int>(packet.opcode) << "\n";
            break;
    }
}

void ClientSession::send_packet(const Packet& packet) {
    std::vector<uint8_t> body = packet.serialize();

    ByteBuffer header;
    header.write_uint16(static_cast<uint16_t>(body.size()));

    std::vector<uint8_t> full_packet = header.data();
    full_packet.insert(full_packet.end(), body.begin(), body.end());

    write_queue_.push_back(std::move(full_packet));
    if (!writing_) {
        do_write();
    }
}

void ClientSession::do_write() {
    if (write_queue_.empty()) {
        writing_ = false;
        return;
    }

    writing_ = true;
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(write_queue_.front()),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                 if (ec) {
                                     std::cerr << "[Server] Write failed: " << ec.message() << "\n";
                                     close();
                                     return;
                                 }

                                 write_queue_.pop_front();
                                 do_write();
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
