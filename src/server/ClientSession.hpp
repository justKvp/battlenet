#pragma once

#include <boost/asio.hpp>
#include <memory>
#include "Packet.hpp"

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket);

    void start();

private:
    void read_header();

    void read_body(std::size_t size);

    void handle_packet(Packet &packet);

    void send_packet(const Packet &packet);

    boost::asio::ip::tcp::socket socket_;
    std::vector<uint8_t> header_buffer_;
    std::vector<uint8_t> body_buffer_;
};
