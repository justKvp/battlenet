#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <queue>
#include "byte_buffer.hpp"
#include "opcodes.hpp"

using boost::asio::ip::tcp;

class AsyncClient : public std::enable_shared_from_this<AsyncClient> {
    tcp::socket socket_;
    boost::asio::steady_timer connect_timer_;
    boost::asio::steady_timer timeout_timer_;
    ByteBuffer read_buffer_;
    std::queue<std::pair<Opcode, ByteBuffer>> write_queue_;
    bool is_writing_ = false;

    void do_connect(const tcp::endpoint& endpoint);
    void do_write();
    void do_read_header();
    void do_read_body(uint16_t opcode, uint32_t body_size);
    void reset_timeout();

public:
    AsyncClient(boost::asio::io_context& io_context)
            : socket_(io_context),
              connect_timer_(io_context),
              timeout_timer_(io_context) {}

    void connect(const std::string& host, const std::string& port);
    void send(Opcode opcode, const ByteBuffer& body);
    void close();
};