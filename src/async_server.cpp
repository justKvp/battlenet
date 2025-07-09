#include "async_server.hpp"

ServerConnection::ServerConnection(tcp::socket socket)
        : socket_(std::move(socket)),
          timeout_timer_(socket_.get_executor()) {}

void ServerConnection::start() {
    if (!socket_.is_open()) return;

    try {
        socket_.set_option(tcp::no_delay(true));
        socket_.set_option(boost::asio::socket_base::keep_alive(true));
        reset_timeout();
        do_read_header();
        std::cout << "Connection started\n";
    } catch (const std::exception& e) {
        std::cerr << "Start error: " << e.what() << "\n";
        close(true);
    }
}

void ServerConnection::close(bool force) {
    if (is_closing_) return;
    is_closing_ = true;

    boost::system::error_code ec;
    timeout_timer_.cancel();
    if (force) socket_.cancel(ec);

    if (socket_.is_open()) {
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    if (on_close_) on_close_();
    std::cout << "Connection closed\n";
}

void ServerConnection::send_packet(Opcode opcode, const ByteBuffer& body) {
    if (!socket_.is_open()) return;

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
                        std::cerr << "[SERVER] Send failed for opcode "
                                  << static_cast<int>(opcode) << ": "
                                  << ec.message() << "\n";
                        close();
                    } else {
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
        send_packet(Opcode::SERVER_INTERNAL_ERROR_RES, response);
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Error sending error message: " << e.what() << "\n";
    }
}

void ServerConnection::reset_timeout() {
    timeout_timer_.expires_after(std::chrono::seconds(30));
    timeout_timer_.async_wait(
            [self = shared_from_this()](boost::system::error_code ec) {
                if (!ec && self->socket_.is_open()) {
                    std::cout << "Connection timeout\n";
                    self->close();
                }
            });
}

void ServerConnection::do_read_header() {
    auto buffer = std::make_shared<std::array<uint8_t, 6>>();

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(*buffer),
            [this, self, buffer](boost::system::error_code ec, size_t bytes) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        std::cerr << "Header read error: " << ec.message() << "\n";
                    }
                    close();
                    return;
                }

                try {
                    uint16_t opcode = *reinterpret_cast<uint16_t*>(buffer->data());
                    uint32_t body_size = *reinterpret_cast<uint32_t*>(buffer->data() + 2);

                    // Исправленная проверка (1 MB вместо 10 MB)
                    if (body_size > 1024 * 1024) {
                        throw std::runtime_error("Packet size exceeds 1 MB limit");
                    }

                    reset_timeout();
                    do_read_body(opcode, body_size);
                } catch (const std::exception& e) {
                    std::cerr << "Header error: " << e.what() << "\n";
                    close(true);
                }
            });
}

void ServerConnection::do_read_body(uint16_t opcode, uint32_t body_size) {
    auto buffer = std::make_shared<std::vector<uint8_t>>(body_size);

    auto self = shared_from_this();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(*buffer),
                            [this, self, buffer, opcode](boost::system::error_code ec, size_t bytes) {
                                if (ec) {
                                    if (ec != boost::asio::error::operation_aborted) {
                                        std::cerr << "Body read error: " << ec.message() << "\n";
                                    }
                                    close();
                                    return;
                                }

                                try {
                                    read_buffer_.clear();
                                    read_buffer_.write(buffer->data(), bytes);
                                    handle_packet(opcode, read_buffer_);

                                    if (socket_.is_open()) {
                                        reset_timeout();
                                        do_read_header();
                                    }
                                } catch (const std::exception& e) {
                                    std::cerr << "Body error: " << e.what() << "\n";
                                    close(true);
                                }
                            });
}

bool ServerConnection::is_validated_packet(const ByteBuffer& body, size_t expected_size) {
    if (body.size() < expected_size) {
        return false;
        //throw std::runtime_error("Invalid packet size");
    }
    return true;
}

void ServerConnection::handle_packet(uint16_t opcode, const ByteBuffer& body) {
    try {
        switch (static_cast<Opcode>(opcode)) {
            case Opcode::CLIENT_AUTH_INIT_REQ: {
                if (!is_validated_packet(body, sizeof(uint32_t))) {
                    break;
                }

                std::string username = body.read_string();

                ByteBuffer response;
                response.write_uint8(1); // success
                response.write_string("Welcome, " + username);
                response.write_uint32(100); // initial token

                send_packet(Opcode::SERVER_AUTH_INIT_RES, response);
                break;
            }

            case Opcode::CLIENT_HELLO_REQ: {

                uint32_t number = body.read_uint32();
                std::string client_message = body.read_string();

                ByteBuffer response;
                response.write_string("Welcome you too =)");

                send_packet(Opcode::SERVER_AUTH_INIT_RES, response);
                break;
            }
            default:
                throw std::runtime_error("Unknown opcode: " + std::to_string(opcode));
        }
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Packet handling error: " << e.what() << "\n";
        send_error(e.what());
        close();
    }
}

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
        conn->close(true);
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
                    std::cout << "New connection. Total: "
                              << active_connections() << "\n";
                }

                if (acceptor_.is_open()) {
                    do_accept();
                }
            });
}

void AsyncServer::remove_connection(const std::shared_ptr<ServerConnection>& conn) {
    connections_.erase(conn);
    std::cout << "Connection removed. Active: "
              << connections_.size() << "\n";
}

size_t AsyncServer::active_connections() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}