#include "../include/async_server.h"
#include <iostream>

// Session implementation
std::shared_ptr<Session> Session::create(tcp::socket socket, AsyncServer& server) {
    return std::shared_ptr<Session>(new Session(std::move(socket), server));
}

Session::Session(tcp::socket socket, AsyncServer& server)
        : socket_(std::move(socket)),
          server_(server),
          read_buffer_(new uint8_t[MAX_PACKET_SIZE]),
          last_activity_(std::chrono::steady_clock::now()) {}

void Session::start() {
    if (!socket_.is_open()) {
        server_.remove_session(shared_from_this());
        return;
    }
    do_read_header();
}

void Session::do_read_header() {
    last_activity_ = std::chrono::steady_clock::now();

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_buffer_.get(), HEADER_SIZE),
            [this, self](boost::system::error_code ec, size_t length) {
                if (ec) {
                    // Не логируем стандартные ошибки закрытия
                    if (ec != boost::asio::error::operation_aborted &&
                        ec != boost::asio::error::eof) {
                        SERVER_LOG("Read header error: " << ec.message());
                    }
                    server_.remove_session(self);
                    return;
                }

                uint16_t packet_size = *reinterpret_cast<uint16_t*>(read_buffer_.get());
                current_opcode_ = *reinterpret_cast<uint16_t*>(read_buffer_.get() + sizeof(uint16_t));

                if (packet_size > MAX_PACKET_SIZE - HEADER_SIZE) {
                    SERVER_LOG("Invalid packet size: " << packet_size << " (max: "
                                                       << (MAX_PACKET_SIZE - HEADER_SIZE) << ")");
                    server_.remove_session(self);
                    return;
                }

                do_read_body(packet_size);
            });
}

void Session::do_read_body(uint16_t size) {
    last_activity_ = std::chrono::steady_clock::now();

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_buffer_.get() + HEADER_SIZE, size),
            [this, self](boost::system::error_code ec, size_t length) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        SERVER_LOG("Read body error: " << ec.message());
                    }
                    server_.remove_session(self);
                    return;
                }

                try {
                    handle_packet();
                    do_read_header();
                } catch (const std::exception& e) {
                    SERVER_LOG("Packet processing error: " << e.what());
                    server_.remove_session(self);
                }
            });
}

void Session::handle_packet() {
    ByteBuffer buffer(read_buffer_.get(), HEADER_SIZE + sizeof(uint16_t));
    buffer.SkipRead(sizeof(uint16_t));

    switch (static_cast<Opcodes>(current_opcode_)) {
        case Opcodes::CMSG_INVENTORY_MOVE: {
            uint8_t bag, slot;
            uint32_t itemId;
            buffer >> bag >> slot >> itemId;
            server_.on_inventory_move(shared_from_this(), bag, slot, itemId);
            break;
        }
        default:
            throw std::runtime_error("Unknown opcode: " + std::to_string(current_opcode_));
    }
}

void Session::send(const ByteBuffer& buffer) {
    if (!socket_.is_open()) return;

    bool write_in_progress = !write_queue_.empty();
    write_queue_.push(buffer);

    if (!write_in_progress) {
        do_write();
    }
}

void Session::do_write() {
    auto self = shared_from_this();
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(write_queue_.front().GetData(),
                                write_queue_.front().GetSize()),
            [this, self](boost::system::error_code ec, size_t /*length*/) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        SERVER_LOG("Write error: " << ec.message());
                    }
                    server_.remove_session(self);
                    return;
                }

                write_queue_.pop();
                if (!write_queue_.empty()) {
                    do_write();
                }
            });
}

void Session::set_error_handler(std::function<void()> handler) {
    error_handler_ = handler;
}

bool Session::is_connected() const {
    return socket_.is_open();
}

std::chrono::steady_clock::time_point Session::get_last_activity() const {
    return last_activity_;
}

tcp::socket& Session::get_socket() {
    return socket_;
}

// AsyncServer implementation
AsyncServer::AsyncServer(boost::asio::io_context& io_context, uint16_t port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    do_accept();
}

AsyncServer::~AsyncServer() {
    stop();
}

void AsyncServer::stop() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    if (stopped_) return;
    stopped_ = true;

    boost::system::error_code ec;
    acceptor_.close(ec);

    for (auto& session : sessions_) {
        session->get_socket().close(ec);
    }
    sessions_.clear();

    SERVER_LOG("Server stopped");
}

void AsyncServer::set_move_handler(std::function<void(uint8_t, uint8_t, uint32_t)> handler) {
    move_handler_ = handler;
}

void AsyncServer::on_inventory_move(std::shared_ptr<Session> session,
                                    uint8_t bag, uint8_t slot, uint32_t itemId) {
    SERVER_LOG("Processing move - Bag:" << (int)bag
                                        << " Slot:" << (int)slot
                                        << " Item:" << itemId);

    if (move_handler_) {
        move_handler_(bag, slot, itemId);
    }

    ByteBuffer response;
    response << static_cast<uint16_t>(Opcodes::SMSG_INVENTORY_MOVE_RESULT);
    response << true; // success
    session->send(response);
}

void AsyncServer::do_accept() {
    if (stopped_) return;

    acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        SERVER_LOG("Accept error: " << ec.message());
                    }
                    return;
                }

                auto session = Session::create(std::move(socket), *this);
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_.insert(session);
                }
                session->start();
                SERVER_LOG("New connection (Total: " << sessions_.size() << ")");

                do_accept();
            });
}

void AsyncServer::remove_session(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    if (sessions_.erase(session)) {
        SERVER_LOG("Connection closed (Remaining: " << sessions_.size() << ")");
    }
}

void AsyncServer::check_dead_connections() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto now = std::chrono::steady_clock::now();
    constexpr auto timeout = std::chrono::seconds(30);

    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        auto session = *it;

        if (!session->is_connected()) {
            it = sessions_.erase(it);
            continue;
        }

        if (now - session->get_last_activity() > timeout) {
            SERVER_LOG("Closing inactive connection");
            session->get_socket().close();
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}