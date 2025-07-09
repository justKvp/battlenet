#pragma once

#include <boost/asio.hpp>
#include <set>
#include <memory>

class ClientSession;

class Server {
public:
    Server(boost::asio::io_context &io_context, int port);

    void start_accept();

    void remove_session(std::shared_ptr<ClientSession> session);

private:
    void handle_accept(std::shared_ptr<ClientSession> session, const boost::system::error_code &ec);

    boost::asio::io_context &io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    std::set<std::shared_ptr<ClientSession>> sessions;
};
