#include "../include/async_client.h"
#include <iostream>
#include <thread>

AsyncClient::AsyncClient(boost::asio::io_context& io_context,
                         const tcp::resolver::results_type& endpoints)
        : socket_(io_context),
          endpoints_(endpoints),
          read_buffer_(new uint8_t[MAX_PACKET_SIZE]),
          timer_(io_context) {}

void AsyncClient::Start(std::function<void()> on_connected) {
    on_connected_ = on_connected;
    DoConnect();
}

void AsyncClient::DoConnect() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (disconnected_) return;

    CLIENT_LOG("Connecting to server...");
    StartConnectionTimer();

    boost::asio::async_connect(
            socket_,
            endpoints_,
            [this](boost::system::error_code ec, const tcp::endpoint& endpoint) {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                timer_.cancel();

                if (ec) {
                    HandleError("Connection failed: " + ec.message());
                    Reconnect();
                    return;
                }

                is_connected_ = true;
                reconnect_attempts_ = 0;
                CLIENT_LOG("Connected to " << endpoint);

                // Start reading immediately
                DoReadHeader();

                if (on_connected_) {
                    try {
                        on_connected_();
                    } catch (const std::exception& e) {
                        HandleError(std::string("Connection callback error: ") + e.what());
                    }
                }
            });
}

void AsyncClient::StartConnectionTimer() {
    timer_.expires_after(std::chrono::seconds(CONNECTION_TIMEOUT));
    timer_.async_wait([this](boost::system::error_code ec) {
        if (!ec) {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (!is_connected_ && !disconnected_) {
                socket_.close();
                HandleError("Connection timeout");
                Reconnect();
            }
        }
    });
}

void AsyncClient::Reconnect() {
    if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        HandleError("Max reconnect attempts reached");
        return;
    }

    reconnect_attempts_++;
    CLIENT_LOG("Reconnecting attempt " << reconnect_attempts_
                                       << "/" << MAX_RECONNECT_ATTEMPTS << "...");

    // Экспоненциальная задержка
    auto delay = std::chrono::seconds(1 << reconnect_attempts_);
    std::this_thread::sleep_for(delay);
    DoConnect();
}

void AsyncClient::SendInventoryMove(uint8_t bag, uint8_t slot, uint32_t itemId) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!is_connected_ || disconnected_) {
        HandleError("Cannot send - not connected to server");
        return;
    }

    ByteBuffer buffer;
    uint16_t packet_size = sizeof(uint16_t) + // opcode
                           sizeof(bag) +
                           sizeof(slot) +
                           sizeof(itemId);

    buffer << packet_size;
    buffer << static_cast<uint16_t>(Opcodes::CMSG_INVENTORY_MOVE);
    buffer << bag;
    buffer << slot;
    buffer << itemId;

    bool write_in_progress = !write_queue_.empty();
    write_queue_.push(buffer);

    if (!write_in_progress) {
        DoWrite();
    }
}

void AsyncClient::DoReadHeader() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (disconnected_ || !is_connected_) return;

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_buffer_.get(), HEADER_SIZE),
            [this, self](boost::system::error_code ec, size_t length) {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                if (ec) {
                    HandleError(ec == boost::asio::error::eof
                                ? "Connection closed by server"
                                : "Header read error: " + ec.message());
                    return;
                }

                uint16_t packet_size = *reinterpret_cast<uint16_t*>(read_buffer_.get());
                if (packet_size > MAX_PACKET_SIZE - HEADER_SIZE) {
                    HandleError("Invalid packet size: " + std::to_string(packet_size));
                    return;
                }

                DoReadBody(packet_size);
            });
}

void AsyncClient::DoReadBody(uint16_t size) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (disconnected_ || !is_connected_) return;

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_buffer_.get() + HEADER_SIZE, size),
            [this, self](boost::system::error_code ec, size_t length) {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                if (ec) {
                    HandleError("Body read error: " + ec.message());
                    return;
                }

                try {
                    ByteBuffer buffer(read_buffer_.get(), HEADER_SIZE + length);
                    buffer.SkipRead(HEADER_SIZE);

                    uint16_t opcode;
                    buffer >> opcode;

                    switch (static_cast<Opcodes>(opcode)) {
                        case Opcodes::SMSG_INVENTORY_MOVE_RESULT: {
                            bool success;
                            buffer >> success;
                            HandleResponse(success);
                            break;
                        }
                        default:
                            throw std::runtime_error("Unknown opcode");
                    }

                    DoReadHeader();
                } catch (const std::exception& e) {
                    HandleError(std::string("Packet error: ") + e.what());
                }
            });
}

void AsyncClient::HandleResponse(bool success) {
    CLIENT_LOG("Received response: " << (success ? "SUCCESS" : "FAILURE"));
    if (response_handler_) {
        try {
            response_handler_(success);
        } catch (const std::exception& e) {
            CLIENT_LOG("Response handler error: " << e.what());
        }
    }
}

void AsyncClient::DoWrite() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (disconnected_ || !is_connected_ || write_queue_.empty()) return;

    auto self = shared_from_this();
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(write_queue_.front().GetData(),
                                write_queue_.front().GetSize()),
            [this, self](boost::system::error_code ec, size_t /*length*/) {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                if (ec) {
                    HandleError("Write error: " + ec.message());
                    return;
                }

                write_queue_.pop();
                if (!write_queue_.empty()) {
                    DoWrite();
                }
            });
}

void AsyncClient::HandleError(const std::string& error_msg) {
    if (disconnected_) return;

    CLIENT_LOG(error_msg);
    Disconnect();

    if (error_handler_) {
        try {
            error_handler_(error_msg);
        } catch (const std::exception& e) {
            CLIENT_LOG("Error handler error: " << e.what());
        }
    }
}

void AsyncClient::SetResponseHandler(std::function<void(bool)> handler) {
    response_handler_ = handler;
}

void AsyncClient::SetErrorHandler(std::function<void(const std::string&)> handler) {
    error_handler_ = handler;
}

void AsyncClient::Disconnect() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (disconnected_) return;

    disconnected_ = true;
    is_connected_ = false;
    boost::system::error_code ec;

    timer_.cancel(ec);
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    // Очищаем очередь сообщений
    std::queue<ByteBuffer> empty;
    std::swap(write_queue_, empty);

    CLIENT_LOG("Disconnected from server");
}