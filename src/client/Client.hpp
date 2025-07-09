#pragma once
#include <boost/asio.hpp>
#include <string>
#include <queue>
#include <mutex>
#include "Packet.hpp"

class Client {
public:
    Client(boost::asio::io_context& io, const std::string& host, int port);
    void run();

private:
    void connect();
    void reconnect();
    void send_ping();
    void send_message(const std::string& msg);
    void flush_queue();
    void queue_packet(const Packet& p);
    void start_receive_loop();
    void handle_packet(const Packet& p);

    boost::asio::io_context& io;
    boost::asio::ip::tcp::socket socket;
    std::string host;
    int port;
    bool connected = false;

    std::mutex queue_mutex;
    std::queue<std::vector<uint8_t>> outgoing_queue;
};
