#pragma once
#include <boost/asio.hpp>
#include <set>
#include <memory>
#include <mutex>

class ClientSession;

class Server {
public:
    Server(boost::asio::io_context& io, short port);
    void remove_session(std::shared_ptr<ClientSession> session);

private:
    void do_accept();
    void monitor_clients();

    boost::asio::ip::tcp::acceptor acceptor;
    std::set<std::shared_ptr<ClientSession>> sessions;
    std::mutex session_mutex;
};
