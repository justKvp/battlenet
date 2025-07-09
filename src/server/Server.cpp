#include "Server.hpp"
#include "ClientSession.hpp"
#include <iostream>

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context &io_context_, int port)
        : io_context(io_context_), acceptor(io_context_, tcp::endpoint(tcp::v4(), port)) {}

void Server::start_accept() {
    auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.async_accept(*socket, [this, socket](boost::system::error_code ec) {
        if (!ec) {
            auto session = std::make_shared<ClientSession>(std::move(*socket), *this);
            sessions.insert(session);
            session->start();
            std::cout << "[Server] New client connected\n";
        } else {
            std::cerr << "[Server] Accept error: " << ec.message() << "\n";
        }
        start_accept();
    });
}

void Server::remove_session(std::shared_ptr<ClientSession> session) {
    sessions.erase(session);
    std::cout << "[Server] Client disconnected\n";
}