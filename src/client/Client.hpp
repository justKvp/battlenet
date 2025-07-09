#pragma once

#include <boost/asio.hpp>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include "Packet.hpp"  // Здесь должны быть определения Packet и Opcode

class Client {
public:
    Client(boost::asio::io_context& io_context, const std::string& host, int port);

    void run();
    void send_message(const std::string& msg);
    void send_ping();

private:
    void connect();
    void schedule_reconnect();
    void start_heartbeat();
    void start_receive_loop();
    void flush_queue();
    void send_packet(const Packet& packet);
    void handle_packet(const Packet& p);

    boost::asio::io_context& io;
    boost::asio::ip::tcp::socket socket;
    std::string host;
    int port;
    bool connected;

    boost::asio::steady_timer reconnect_timer;
    boost::asio::steady_timer heartbeat_timer;

    std::queue<std::vector<uint8_t>> outgoing_queue;
};
