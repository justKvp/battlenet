#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>
#include <queue>
#include <functional>
#include <mutex>
#include <chrono>
#include "byte_buffer.h"
#include "opcodes.h"

#define SERVER_LOG(msg) std::cout << "[SERVER][" << __TIME__ << "] " << msg << "\n"

using boost::asio::ip::tcp;

class AsyncServer;

class Session : public std::enable_shared_from_this<Session> {
public:
    static std::shared_ptr<Session> create(tcp::socket socket, AsyncServer& server);

    void start();
    void send(const ByteBuffer& buffer);
    tcp::socket& get_socket();
    void set_error_handler(std::function<void()> handler);
    bool is_connected() const;
    std::chrono::steady_clock::time_point get_last_activity() const;

private:
    Session(tcp::socket socket, AsyncServer& server);

    void do_read_header();
    void do_read_body(uint16_t size);
    void handle_packet();
    void do_write();
    void close_connection();

    static constexpr size_t HEADER_SIZE = sizeof(uint16_t) + sizeof(uint16_t);
    static constexpr size_t MAX_PACKET_SIZE = 4096;

    tcp::socket socket_;
    AsyncServer& server_;
    std::unique_ptr<uint8_t[]> read_buffer_;
    std::queue<ByteBuffer> write_queue_;
    uint16_t current_opcode_;
    std::function<void()> error_handler_;
    std::chrono::steady_clock::time_point last_activity_;
};

class AsyncServer {
public:
    AsyncServer(boost::asio::io_context& io_context, uint16_t port);
    ~AsyncServer();

    void on_inventory_move(std::shared_ptr<Session> session,
                           uint8_t bag, uint8_t slot, uint32_t itemId);
    void stop();
    void set_move_handler(std::function<void(uint8_t, uint8_t, uint32_t)> handler);
    void check_dead_connections();

private:
    friend class Session;

    void do_accept();
    void remove_session(std::shared_ptr<Session> session);

    tcp::acceptor acceptor_;
    std::unordered_set<std::shared_ptr<Session>> sessions_;
    std::function<void(uint8_t, uint8_t, uint32_t)> move_handler_;
    bool stopped_ = false;
    std::mutex sessions_mutex_;
};