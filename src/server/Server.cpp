#include "Server.hpp"
#include "ClientSession.hpp"
#include <iostream>

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context& io_context, int port)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    start_accept();
}

void Server::start_accept() {
    auto socket = std::make_shared<tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
        if (!ec) {
            auto session = std::make_shared<ClientSession>(std::move(*socket), shared_from_this());
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.insert(session);
            }
            session->start();
        } else {
            std::cerr << "[Server] Accept error: " << ec.message() << "\n";
        }
        start_accept();
    });
}

void Server::remove_session(std::shared_ptr<ClientSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session);
}
