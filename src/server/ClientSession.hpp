#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include "Packet.hpp"

class Server;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket, Server &server);

    void start();

    void send_packet(const Packet &packet);

private:
    void read_header();

    void read_body(std::size_t length);

    void handle_packet(Packet &packet);

    boost::asio::ip::tcp::socket socket;
    Server &server;
    std::vector<uint8_t> read_buffer;
};
