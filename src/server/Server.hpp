#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <set>
#include <deque>
#include "Packet.hpp"

class Server;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::ip::tcp::socket socket, Server& server);
    void start();
    void send_packet(const Packet& packet);

private:
    void read_header();
    void read_body(std::size_t length);
    void handle_packet(Packet& packet);
    void do_write();

    boost::asio::ip::tcp::socket socket;
    std::vector<uint8_t> read_buffer;
    std::deque<std::vector<uint8_t>> write_queue;
    Server& server;
};

class Server {
public:
    Server(boost::asio::io_context& io_context, int port);
    void start_accept();
    void remove_session(std::shared_ptr<Session> session);

private:
    void handle_accept(std::shared_ptr<Session> session, const boost::system::error_code& error);

    boost::asio::ip::tcp::acceptor acceptor;
    std::set<std::shared_ptr<Session>> sessions;
};
