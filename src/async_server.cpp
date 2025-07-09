#include "async_server.hpp"
#include <iostream>

void ServerConnection::start() {
    if (is_closing_) return;

    try {
        socket_.set_option(tcp::no_delay(true));
        socket_.set_option(boost::asio::socket_base::keep_alive(true));
        reset_timeout();
        do_read_header();
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Start error: " << e.what() << "\n";
        close();
    }
}

void ServerConnection::send(Opcode opcode, const ByteBuffer& body) {
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (is_closing_ || !socket_.is_open()) {
        std::cerr << "[SERVER] Attempt to send on closed connection (opcode: "
                  << static_cast<int>(opcode) << ")\n";
        return;
    }

    try {
        ByteBuffer packet;
        packet.write_uint16(static_cast<uint16_t>(opcode));
        packet.write_uint32(static_cast<uint32_t>(body.size()));
        packet.write(body.data(), body.size());

        auto self = shared_from_this();
        boost::asio::async_write(
                socket_,
                boost::asio::buffer(packet.data(), packet.size()),
                [this, self, opcode](boost::system::error_code ec, size_t bytes) {
                    if (ec) {
                        if (!is_closing_) {
                            std::cerr << "[SERVER] Send failed for opcode "
                                      << static_cast<int>(opcode) << ": "
                                      << ec.message() << "\n";
                        }
                        close();
                    } else if (!is_closing_) {
                        std::cout << "[SERVER] Sent opcode "
                                  << static_cast<int>(opcode)
                                  << ", " << bytes << " bytes\n";
                    }
                });
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Packet assembly error: " << e.what() << "\n";
        close();
    }
}

void ServerConnection::send_error(const std::string& message) {
    try {
        ByteBuffer response;
        response.write_uint8(0); // error flag
        response.write_string(message);
        send(Opcode::SERVER_INTERNAL_ERROR_RES, response);
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Error sending error message: " << e.what() << "\n";
    }
}

void ServerConnection::close() {
    if (is_closing_.exchange(true)) {
        return;
    }

    boost::system::error_code ec;
    timeout_timer_.cancel();
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    if (on_close_) {
        on_close_();
    }
}

void ServerConnection::reset_timeout() {
    if (is_closing_) return;

    timeout_timer_.expires_after(std::chrono::seconds(30));
    timeout_timer_.async_wait(
            [self = shared_from_this()](boost::system::error_code ec) {
                if (!ec && !self->is_closing_) {
                    self->close();
                }
            });
}

void ServerConnection::do_read_header() {
    if (is_closing_) return;

    auto buffer = std::make_shared<std::array<uint8_t, 6>>();

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(*buffer),
            [this, self, buffer](boost::system::error_code ec, size_t bytes) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted && !is_closing_) {
                        std::cerr << "[SERVER] Header read error: " << ec.message() << "\n";
                    }
                    close();
                    return;
                }

                try {
                    uint16_t opcode = *reinterpret_cast<uint16_t*>(buffer->data());
                    uint32_t body_size = *reinterpret_cast<uint32_t*>(buffer->data() + 2);

                    if (body_size > 1024 * 1024) {
                        throw std::runtime_error("Packet too large");
                    }

                    reset_timeout();
                    do_read_body(opcode, body_size);
                } catch (const std::exception& e) {
                    std::cerr << "[SERVER] Header error: " << e.what() << "\n";
                    send_error(e.what());
                    close();
                }
            });
}

void ServerConnection::do_read_body(uint16_t opcode, uint32_t body_size) {
    if (is_closing_) return;

    auto buffer = std::make_shared<std::vector<uint8_t>>(body_size);

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(*buffer),
            [this, self, buffer, opcode](boost::system::error_code ec, size_t bytes) {
                if (ec) {
                    if (!is_closing_) {
                        std::cerr << "[SERVER] Body read error: " << ec.message() << "\n";
                    }
                    close();
                    return;
                }

                try {
                    ByteBuffer body;
                    body.write(buffer->data(), bytes);
                    handle_packet(opcode, body);

                    if (!is_closing_) {
                        reset_timeout();
                        do_read_header();
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[SERVER] Body error: " << e.what() << "\n";
                    send_error(e.what());
                    close();
                }
            });
}

void ServerConnection::handle_packet(uint16_t opcode, const ByteBuffer& body) {
    if (is_closing_) return;

    try {
        switch (static_cast<Opcode>(opcode)) {
            case Opcode::CLIENT_AUTH_INIT_REQ: {
                std::string username = body.read_string();
                std::cout << "[SERVER] CLIENT_AUTH_INIT_REQ from: " << username << "\n";

                ByteBuffer response;
                response.write_uint8(1); // success
                response.write_string("Welcome, " + username);
                send(Opcode::SERVER_AUTH_INIT_RES, response);
                break;
            }

            case Opcode::CLIENT_HELLO_REQ: {
                std::cout << "[SERVER] CLIENT_HELLO_REQ handled\n";
//                uint32_t request_id = body.read_uint32();
//                std::string client_message = body.read_string();

//                ByteBuffer response;
//                response.write_string("Welcome you too =)");
//                send(Opcode::SERVER_AUTH_INIT_RES, response);
                break;
            }

            default:
                throw std::runtime_error("Unknown opcode: " + std::to_string(opcode));
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Handle packet: ") + e.what());
    }
}

// AsyncServer implementation
AsyncServer::AsyncServer(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          io_context_(io_context) {}

void AsyncServer::start() {
    do_accept();
}

void AsyncServer::stop() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    acceptor_.close();

    for (auto& conn : connections_) {
        conn->close();
    }
    connections_.clear();
}

void AsyncServer::do_accept() {
    acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto conn = std::make_shared<ServerConnection>(std::move(socket));

                    {
                        std::lock_guard<std::mutex> lock(connections_mutex_);
                        connections_.insert(conn);
                    }

                    conn->set_on_close([this, conn]() {
                        std::lock_guard<std::mutex> lock(connections_mutex_);
                        remove_connection(conn);
                    });

                    conn->start();
                    std::cout << "[SERVER] New connection. Total: "
                              << active_connections() << "\n";
                }

                if (acceptor_.is_open()) {
                    do_accept();
                }
            });
}

void AsyncServer::remove_connection(const std::shared_ptr<ServerConnection>& conn) {
    connections_.erase(conn);
    std::cout << "[SERVER] Connection removed. Active: "
              << connections_.size() << "\n";
}

size_t AsyncServer::active_connections() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}