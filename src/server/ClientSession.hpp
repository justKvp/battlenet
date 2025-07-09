#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <deque>

#include "Packet.hpp"

class Server;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    explicit ClientSession(boost::asio::ip::tcp::socket socket, std::shared_ptr<Server> server);

    void start();
    void send_packet(const Packet& packet);
    void close();

private:
    void read_header();
    void read_body(std::size_t size);
    void handle_packet(Packet& packet);
    void do_write();

    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<Server> server_;

    std::vector<uint8_t> header_buffer_;
    std::vector<uint8_t> body_buffer_;

    std::deque<std::vector<uint8_t>> write_queue_;
    bool writing_ = false;
};
