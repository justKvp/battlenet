#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <chrono>
#include "Packet.hpp"

class Server;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket, Server& server);

    void start();
    void check_timeout(std::chrono::steady_clock::time_point now);

    boost::asio::ip::tcp::socket socket;

private:
    void read_header();
    void read_body(std::size_t length);
    void handle_packet(Packet& packet);

    Server& server;
    std::vector<uint8_t> buffer;
    std::chrono::steady_clock::time_point last_ping;
};
