#include "Server.hpp"
#include <iostream>

using boost::asio::ip::tcp;

Session::Session(tcp::socket socket_, Server& server_)
        : socket(std::move(socket_)), server(server_) {}

void Session::start() {
    read_header();
}

void Session::read_header() {
    auto self(shared_from_this());
    read_buffer.resize(2);
    boost::asio::async_read(socket, boost::asio::buffer(read_buffer),
                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                if (!ec) {
                                    uint16_t size = (read_buffer[0] << 8) | read_buffer[1];
                                    read_body(size);
                                } else {
                                    std::cerr << "[Server] Read header error: " << ec.message() << "\n";
                                    server.remove_session(self);
                                }
                            });
}

void Session::read_body(std::size_t length) {
    auto self(shared_from_this());
    read_buffer.resize(length);
    boost::asio::async_read(socket, boost::asio::buffer(read_buffer),
                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                if (!ec) {
                                    try {
                                        Packet p = Packet::deserialize(read_buffer);
                                        handle_packet(p);
                                    } catch (...) {
                                        std::cerr << "[Server] Failed to parse packet\n";
                                    }
                                    read_header();
                                } else {
                                    std::cerr << "[Server] Read body error: " << ec.message() << "\n";
                                    server.remove_session(self);
                                }
                            });
}

void Session::handle_packet(Packet& packet) {
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

void Session::send_packet(const Packet& packet) {
    auto self(shared_from_this());
    std::vector<uint8_t> data = packet.serialize();
    write_queue.push_back(data);

    if (write_queue.size() == 1) {
        do_write();
    }
}

void Session::do_write() {
    auto self(shared_from_this());
    boost::asio::async_write(socket, boost::asio::buffer(write_queue.front()),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                 if (!ec) {
                                     write_queue.pop_front();
                                     if (!write_queue.empty()) {
                                         do_write();
                                     }
                                 } else {
                                     std::cerr << "[Server] Write error: " << ec.message() << "\n";
                                     server.remove_session(self);
                                 }
                             });
}

Server::Server(boost::asio::io_context& io_context, int port)
        : acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
    start_accept();
}

void Server::start_accept() {
    auto socket = std::make_shared<tcp::socket>(acceptor.get_executor());
    acceptor.async_accept(*socket,
                          [this, socket](boost::system::error_code ec) {
                              if (!ec) {
                                  auto session = std::make_shared<Session>(std::move(*socket), *this);
                                  sessions.insert(session);
                                  session->start();
                              }
                              start_accept();
                          });
}

void Server::remove_session(std::shared_ptr<Session> session) {
    sessions.erase(session);
}
