#include "ClientSession.hpp"
#include "Server.hpp"
#include <iostream>

ClientSession::ClientSession(boost::asio::ip::tcp::socket s, Server& srv)
        : socket(std::move(s)), server(srv) {
    last_ping = std::chrono::steady_clock::now();
}

void ClientSession::start() {
    read_header();
}

void ClientSession::read_header() {
    auto self = shared_from_this();
    buffer.resize(2);
    boost::asio::async_read(socket, boost::asio::buffer(buffer),
                            [this, self](boost::system::error_code ec, std::size_t) {
                                if (ec) {
                                    server.remove_session(self);
                                    return;
                                }

                                uint16_t size = (buffer[0] << 8) | buffer[1];
                                read_body(size);
                            });
}

void ClientSession::read_body(std::size_t length) {
    auto self = shared_from_this();
    buffer.resize(length);
    boost::asio::async_read(socket, boost::asio::buffer(buffer),
                            [this, self](boost::system::error_code ec, std::size_t) {
                                if (ec) {
                                    server.remove_session(self);
                                    return;
                                }

                                try {
                                    Packet p = Packet::deserialize(buffer);
                                    handle_packet(p);
                                } catch (...) {
                                    std::cerr << "[Server] Failed to deserialize packet\n";
                                }

                                read_header();
                            });
}

void ClientSession::handle_packet(Packet& p) {
    switch (p.opcode) {
        case Opcode::PING:
            last_ping = std::chrono::steady_clock::now();
            std::cout << "[Server] Received PING\n";

            // Send back PONG
            {
                Packet pong;
                pong.opcode = Opcode::PONG;
                auto data = pong.serialize();
                boost::asio::async_write(socket, boost::asio::buffer(data),
                                         [self = shared_from_this()](boost::system::error_code, std::size_t) {});
            }
            break;

        case Opcode::MESSAGE:
            std::cout << "[Client] " << p.buffer.read_string() << "\n";
            break;

        default:
            std::cerr << "[Server] Unknown opcode\n";
    }
}

void ClientSession::check_timeout(std::chrono::steady_clock::time_point now) {
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_ping).count() > 15) {
        std::cerr << "[Server] Client timed out\n";
        socket.close();
    }
}
