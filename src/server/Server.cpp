#include "Server.hpp"
#include "ClientSession.hpp"
#include <iostream>

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context& io_context, int port)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    start_accept();
}

void Server::start_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
        if (!ec) {
            auto session = std::make_shared<ClientSession>(std::move(socket), shared_from_this());
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.insert(session);
            }
            std::cout << "[Server] New client connected.\n";
            log_session_count();
            session->start();
        }
        start_accept();
    });
}

void Server::remove_session(std::shared_ptr<ClientSession> session) {
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session);
    }
    std::cout << "[Server] Client disconnected.\n";
    log_session_count();
}

void Server::log_session_count() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::cout << "[Server] Active sessions: " << sessions_.size() << "\n";
}