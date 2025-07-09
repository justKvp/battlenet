#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>
#include <mutex>
#include "ClientSession/ClientSession.hpp"

class ClientSession;

class Server : public std::enable_shared_from_this<Server> {
public:
    Server(boost::asio::any_io_executor executor, int port);

    void start_accept(); // <-- теперь публичный!
    void stop();

    void remove_session(std::shared_ptr<ClientSession> session);
    void log_session_count();

private:
    boost::asio::any_io_executor executor_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_set<std::shared_ptr<ClientSession>> sessions_;
    std::mutex sessions_mutex_;
};
