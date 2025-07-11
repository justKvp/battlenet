#include "Server.hpp"
#include "ClientSession/ClientSession.hpp"

#include <iostream>

using boost::asio::ip::tcp;

Server::Server(boost::asio::any_io_executor executor,
               std::shared_ptr<Database> db,
               int port)
        : executor_(executor),
          acceptor_(executor, tcp::endpoint(tcp::v4(), port)),
          db_(std::move(db)) {
}

void Server::start_accept() {
    // Сохраняем shared_from_this внутри лямбды, чтобы Server не разрушился
    auto self = shared_from_this();

    acceptor_.async_accept(
            [self](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<ClientSession>(std::move(socket), self);

                    {
                        std::lock_guard<std::mutex> lock(self->sessions_mutex_);
                        self->sessions_.insert(session);
                        std::cout << "[Server] New client connected.\n";
                        self->log_session_count();
                    }

                    session->start();

                } else {
                    std::cerr << "[Server] Accept failed: " << ec.message() << "\n";
                }

                if (self->acceptor_.is_open()) {
                    self->start_accept();
                }
            });
}

void Server::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
    if (ec) {
        std::cerr << "[Server] Failed to close acceptor: " << ec.message() << "\n";
    } else {
        std::cout << "[Server] Acceptor closed.\n";
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto &session: sessions_) {
        session->close();
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
