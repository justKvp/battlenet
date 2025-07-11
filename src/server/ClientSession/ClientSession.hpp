#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <deque>

#include "Packet.hpp"

#include "src/server/Server.hpp"

class Server; // forward declaration

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket, std::shared_ptr<Server> server);

    void start();
    void close();

    // Позволяет запускать задачи в thread_pool сервера
    template <typename Func>
    void post(Func&& func);

    template <typename Func>
    void spawn(Func&& func);

private:
    void read_header();
    void read_body(std::size_t size);
    void handle_packet(Packet& packet);
    void send_packet(const Packet& packet);
    void do_write();

    boost::asio::ip::tcp::socket socket_;
    std::vector<uint8_t> header_buffer_;
    std::vector<uint8_t> body_buffer_;
    std::shared_ptr<Server> server_;

    std::deque<std::vector<uint8_t>> write_queue_;
    bool writing_ = false;
};
