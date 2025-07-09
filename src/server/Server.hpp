#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>
#include <mutex>
#include "ClientSession/ClientSession.hpp"

class ClientSession; // forward declaration

class Server : public std::enable_shared_from_this<Server> {
public:
    Server(boost::asio::io_context& io_context, int port);
    void remove_session(std::shared_ptr<ClientSession> session);
    void log_session_count();

private:
    void start_accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_set<std::shared_ptr<ClientSession>> sessions_;
    std::mutex sessions_mutex_;
    boost::asio::io_context& io_context_;
};
