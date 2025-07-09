#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>
#include <functional>
#include <iostream>
#include <mutex>
#include "byte_buffer.hpp"
#include "opcodes.hpp"

using boost::asio::ip::tcp;

class ServerConnection : public std::enable_shared_from_this<ServerConnection> {
    tcp::socket socket_;
    ByteBuffer read_buffer_;
    boost::asio::steady_timer timeout_timer_;
    std::function<void()> on_close_;
    bool is_closing_ = false;

    void do_read_header();
    void do_read_body(uint16_t opcode, uint32_t body_size);
    void handle_packet(uint16_t opcode, const ByteBuffer& body);
    void reset_timeout();

public:
    ServerConnection(tcp::socket socket);

    void set_on_close(std::function<void()> callback) {
        on_close_ = std::move(callback);
    }

    void start();
    void close(bool force = false);
};

class AsyncServer {
    tcp::acceptor acceptor_;
    boost::asio::io_context& io_context_;
    std::unordered_set<std::shared_ptr<ServerConnection>> connections_;
    mutable std::mutex connections_mutex_;

    void do_accept();
    void remove_connection(const std::shared_ptr<ServerConnection>& connection);

public:
    AsyncServer(boost::asio::io_context& io_context, short port);
    void start();
    void stop();
    size_t active_connections() const;
};