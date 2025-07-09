#include "Server.hpp"
#include "ClientSession/ClientSession.hpp"

#include <iostream>

using boost::asio::ip::tcp;

Server::Server(boost::asio::any_io_executor executor, int port)
        : executor_(executor),
          acceptor_(executor, tcp::endpoint(tcp::v4(), port)) {
    start_accept();
}

void Server::start_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto session = std::make_shared<ClientSession>(std::move(socket), shared_from_this());

            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.insert(session);
                std::cout << "[Server] New client connected.\n";
                log_session_count();
            }

            session->start();
        } else {
            std::cerr << "[Server] Accept failed: " << ec.message() << "\n";
        }

        if (acceptor_.is_open()) {
            start_accept();
        }
    });
}

void Server::stop() {
    // Закрыть acceptor (чтобы прекратить принимать новые соединения)
    boost::system::error_code ec;
    acceptor_.close(ec);
    if (ec) {
        std::cerr << "[Server] Failed to close acceptor: " << ec.message() << "\n";
    } else {
        std::cout << "[Server] Acceptor closed.\n";
    }

    // Завершить все активные сессии
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& session : sessions_) {
        session->close(); // ClientSession::close() уже есть
    }
    sessions_.clear();
    log_session_count();
}

void Server::remove_session(std::shared_ptr<ClientSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session);
    log_session_count();
}

void Server::log_session_count() {
    std::cout << "[Server] Active sessions: " << sessions_.size() << "\n";
}
