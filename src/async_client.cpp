#include "../include/async_client.h"
#include <boost/asio/steady_timer.hpp>
#include <iostream>

using namespace boost::asio;
using namespace std::chrono_literals;

AsyncClient::AsyncClient(const std::string& host, const std::string& port)
        : host_(host), port_(port),
          socket_(io_context_),
          resolver_(io_context_),
          read_buffer_(1024) {

    worker_thread_ = std::thread([this]() {
        io_context_.run();
    });
}

AsyncClient::~AsyncClient() {
    io_context_.stop();
    if (socket_.is_open()) {
        socket_.close();
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void AsyncClient::Connect() {
    auto timer = std::make_shared<steady_timer>(io_context_);
    timer->expires_after(5s);

    timer->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            std::cerr << "[CLIENT] Connection timeout\n";
            timeout_occurred_ = true;
            socket_.close();
        }
    });

    resolver_.async_resolve(host_, port_,
                            [this, timer](const boost::system::error_code& ec,
                                          ip::tcp::resolver::results_type endpoints) {
                                if (ec) {
                                    std::cerr << "[CLIENT] Resolve error: " << ec.message() << "\n";
                                    timer->cancel();
                                    return;
                                }

                                timer->expires_after(3s);
                                async_connect(socket_, endpoints,
                                              [this, timer](const boost::system::error_code& ec,
                                                            const ip::tcp::endpoint& endpoint) {
                                                  timer->cancel();
                                                  if (ec) {
                                                      std::cerr << "[CLIENT] Connect error: " << ec.message() << "\n";
                                                      return;
                                                  }

                                                  {
                                                      std::lock_guard<std::mutex> lock(connection_mutex_);
                                                      connected_ = true;
                                                      timeout_occurred_ = false;
                                                  }
                                                  std::cout << "[CLIENT] Connected to " << endpoint << "\n";
                                                  StartRead();
                                              });
                            });
}

void AsyncClient::Send(const ByteBuffer& data) {
    if (timeout_occurred_) {
        Reconnect();
    }

    if (!IsConnected()) {
        std::cerr << "[CLIENT] Not connected, cannot send\n";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        outgoing_queue_.push(data);
    }

    if (!writing_) {
        StartWrite();
    }
}

void AsyncClient::StartWrite() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (outgoing_queue_.empty()) return;

    auto timer = std::make_shared<steady_timer>(io_context_);
    timer->expires_after(2s);

    writing_ = true;
    const auto& data = outgoing_queue_.front();

    async_write(socket_, buffer(data.Data(), data.Size()),
                [this, timer](const boost::system::error_code& ec, size_t bytes) {
                    timer->cancel();
                    writing_ = false;
                    if (!ec) {
                        std::cout << "[CLIENT] Sent " << bytes << " bytes\n";
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        outgoing_queue_.pop();
                        if (!outgoing_queue_.empty()) {
                            StartWrite();
                        }
                    } else {
                        std::cerr << "[CLIENT] Write error: " << ec.message() << "\n";
                        std::lock_guard<std::mutex> lock(connection_mutex_);
                        connected_ = false;
                    }
                });

    timer->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            std::cerr << "[CLIENT] Write timeout\n";
            socket_.close();
            timeout_occurred_ = true;
        }
    });
}

void AsyncClient::StartRead() {
    socket_.async_read_some(buffer(read_buffer_.Data(), read_buffer_.Capacity()),
                            [this](const boost::system::error_code& ec, size_t bytes) {
                                if (!ec) {
                                    read_buffer_.SetSize(bytes);
                                    std::cout << "[CLIENT] Received " << bytes << " bytes\n";
                                    StartRead();
                                } else if (ec != error::eof) {
                                    std::cerr << "[CLIENT] Read error: " << ec.message() << "\n";
                                }
                            });
}

bool AsyncClient::IsConnected() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return connected_;
}

void AsyncClient::Reconnect() {
    if (socket_.is_open()) {
        socket_.close();
    }
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connected_ = false;
    }
    Connect();
}

void AsyncClient::SendInventoryMove(int from, int to, int item) {
    ByteBuffer buffer(16);
    buffer.WriteInt32(1);  // Явно указываем opcode=1
    buffer.WriteInt32(from);
    buffer.WriteInt32(to);
    buffer.WriteInt32(item);

    // Отладочный вывод
    std::cout << "Sending opcode: " << buffer.ReadInt32() << "\n";
    Send(buffer);
}