#pragma once

#include <boost/asio.hpp>
#include <queue>
#include <string>
#include <vector>
#include "Packet.hpp"

class Client {
public:
    Client(boost::asio::io_context &io_context, const std::string &host, int port);

    void connect();

    void disconnect();

    void send_message(const std::string &msg);

    void send_hello(const std::string &msg, const uint8_t &number, const float &value);

private:

    void schedule_reconnect();

    void start_heartbeat();

    void send_ping();

    void send_packet(const Packet &packet);

    void flush_queue();

    void start_receive_loop();

    void handle_packet(const Packet &packet);

    boost::asio::io_context &io;
    boost::asio::ip::tcp::socket socket;
    std::string host;
    int port;
    bool connected;

    boost::asio::steady_timer reconnect_timer;
    boost::asio::steady_timer heartbeat_timer;

    std::queue<std::vector<uint8_t>> outgoing_queue;
};
