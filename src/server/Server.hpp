#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>
#include <mutex>

#include "Database.hpp"
#include "ClientSession/ClientSession.hpp"

class ClientSession; // Forward, хотя подключение есть — можно убрать, но не мешает

class Server : public std::enable_shared_from_this<Server> {
public:
    Server(boost::asio::any_io_executor executor,
           std::shared_ptr<Database> db,
           int port);

    void start_accept();

    void stop();

    void remove_session(std::shared_ptr<ClientSession> session);

    void log_session_count();

    // Доступ к общему thread_pool через Database
    boost::asio::thread_pool &thread_pool() { return db_->thread_pool(); }

    // Доступ к БД
    std::shared_ptr<Database> db() { return db_; }

private:
    boost::asio::any_io_executor executor_;
    boost::asio::ip::tcp::acceptor acceptor_;

    std::shared_ptr<Database> db_;  // <- Главное, shared_ptr!

    std::unordered_set<std::shared_ptr<ClientSession>> sessions_;
    std::mutex sessions_mutex_;
};
