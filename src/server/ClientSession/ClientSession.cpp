#include "ClientSession.hpp"
#include "QueryResults.hpp"
#include <iostream>

using boost::asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket, std::shared_ptr<Server> server)
        : socket_(std::move(socket)), server_(std::move(server)) {}

void ClientSession::start() {
    read_header();
}

template <typename Func>
void ClientSession::post(Func&& func) {
    auto self = shared_from_this();
    boost::asio::post(server_->thread_pool(), [self, func = std::forward<Func>(func)] {
        func();
    });
}

template <typename Func>
void ClientSession::spawn(Func&& func) {
    auto self = shared_from_this();
    boost::asio::co_spawn(
            socket_.get_executor(),
            [self, func = std::forward<Func>(func)] {
                return func();
            },
            boost::asio::detached
    );
}

void ClientSession::read_header() {
    header_buffer_.resize(2);
    auto self = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(header_buffer_),
                            [this, self](boost::system::error_code ec, std::size_t) {
                                if (ec) {
                                    if (ec == boost::asio::error::eof) {
                                        std::cout << "[Server] Client disconnected\n";
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
                            [this, self](boost::system::error_code ec, std::size_t) {
                                if (ec) {
                                    if (ec == boost::asio::error::eof) {
                                        std::cout << "[Server] Client disconnected\n";
                                    } else {
                                        std::cerr << "[Server] Body read failed: " << ec.message() << "\n";
                                    }
                                    close();
                                    return;
                                }

                                try {
                                    Packet packet = Packet::deserialize(body_buffer_);
                                    handle_packet(packet);
                                } catch (const std::exception &ex) {
                                    std::cerr << "[Server] Packet deserialization failed: " << ex.what() << "\n";
                                }

                                read_header(); // Слушаем дальше
                            });
}

void ClientSession::handle_packet(Packet &packet) {
    switch (packet.opcode) {
        case Opcode::MESSAGE: {
            std::string msg = packet.buffer.read_string();
            std::cout << "[Server] Message: " << msg << "\n";
            break;
        }
        case Opcode::CMSG_PING: {
            std::cout << "[Server] CMSG_PING received\n";
            Packet pong;
            pong.opcode = Opcode::SMSG_PONG;
            send_packet(pong);
            break;
        }
        case Opcode::CMSG_HELLO: {
            std::string msg = packet.buffer.read_string();
            std::cout << "[Server] CMSG_HELLO received, msg: " << msg << "\n";

            Packet resp;
            resp.opcode = Opcode::SMSG_HELLO_RES;
            resp.buffer.write_string("Hello back!");
            send_packet(resp);
            break;
        }
        case Opcode::CMSG_DATABASE_ASYNC_EXAMPLE: {
            std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode)
                      << "] CMSG_DATABASE_ASYNC_EXAMPLE\n";

            // АСИНХРОННЫЙ ЗАПРОС
            auto self = shared_from_this();
            async_query([self]() -> boost::asio::awaitable<void> {
                try {
                    PreparedStatement stmt("LOGIN_SEL_ACCOUNT_BY_ID");
                    stmt.set_param(0, 1);

                    UserRow user = co_await self->server_->db()->Async.execute_prepared<UserRow>(stmt);

                    std::cout << "[Server][Async] id: " << user.id
                              << ", name: " << user.name << "\n";

                } catch (const std::exception &ex) {
                    std::cerr << "[Server] Async DB error: " << ex.what() << "\n";
                }
                co_return;
            });

            Packet resp;
            resp.opcode = Opcode::SMSG_DATABASE_ASYNC_EXAMPLE;
            send_packet(resp);

            break;
        }
        case Opcode::CMSG_DATABASE_SYNC_EXAMPLE: {
            std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode)
                      << "] CMSG_DATABASE_SYNC_EXAMPLE\n";

            // СИНХРОННЫЙ ЗАПРОС
            auto self = shared_from_this();
            blocking_query([self]() {
                try {
                    PreparedStatement stmt("LOGIN_SEL_ACCOUNT_BY_ID");
                    stmt.set_param(0, 2);

                    UserRow user = self->server_->db()->Sync.execute_prepared<UserRow>(stmt);

                    std::cout << "[Server][Sync] id: " << user.id
                              << ", name: " << user.name << "\n";

                } catch (const std::exception &ex) {
                    std::cerr << "[Server] Sync DB error: " << ex.what() << "\n";
                }
            });

            Packet resp;
            resp.opcode = Opcode::SMSG_DATABASE_SYNC_EXAMPLE;
            send_packet(resp);
            break;
        }
        case Opcode::CMSG_DATABASE_ASYNC_UPDATE: {
            std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode)
                      << "] CMSG_DATABASE_ASYNC_UPDATE\n";

            // Асинхронный без возврата значений
            auto self = shared_from_this();
            async_query([self]() -> boost::asio::awaitable<void> {
                try {
                    PreparedStatement stmt("UPDATE_SOMETHING");
                    stmt.set_param(0, 1);
                    co_await self->server_->db()->Async.execute_prepared<NothingRow>(stmt);

                } catch (const std::exception &ex) {
                    std::cerr << "[Server] Async DB error: " << ex.what() << "\n";
                }
                co_return;
            });

            Packet resp;
            resp.opcode = Opcode::SMSG_DATABASE_ASYNC_UPDATE;
            send_packet(resp);
            break;
        }
        default:
            std::cout << "[Server] Unknown opcode: " << static_cast<int>(packet.opcode) << "\n";
            break;
    }
}

void ClientSession::send_packet(const Packet &packet) {
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
                             [this, self](boost::system::error_code ec, std::size_t) {
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
