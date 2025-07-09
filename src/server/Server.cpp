#include "Server.hpp"
#include "ClientSession.hpp"
#include <thread>

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context &io, short port)
        : acceptor(io, tcp::endpoint(tcp::v4(), port)) {
    do_accept();

    std::thread([this]() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(session_mutex);
                auto now = std::chrono::steady_clock::now();
                for (auto it = sessions.begin(); it != sessions.end();) {
                    auto session = *it;
                    session->check_timeout(now);
                    if (!session->socket.is_open()) {
                        it = sessions.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();
}

void Server::do_accept() {
    acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto session = std::make_shared<ClientSession>(std::move(socket), *this);
            {
                std::lock_guard<std::mutex> lock(session_mutex);
                sessions.insert(session);
            }
            session->start();
        }
        do_accept();
    });
}

void Server::remove_session(std::shared_ptr<ClientSession> session) {
    std::lock_guard<std::mutex> lock(session_mutex);
    sessions.erase(session);
}
