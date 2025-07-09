#include "../include/async_server.h"
#include <iostream>

class AsyncServer::ClientSession : public std::enable_shared_from_this<AsyncServer::ClientSession> {
public:
    ClientSession(tcp::socket socket,
                  const std::unordered_map<uint32_t, AsyncServer::ClientHandler>& handlers)
            : socket_(std::move(socket)), handlers_(handlers),
              buffer_(std::make_unique<ByteBuffer>(1924)) {}

    void Start() {
        DoRead();
    }

private:
    void DoRead() {
        auto self = shared_from_this();
        socket_.async_read_some(
                boost::asio::buffer(buffer_->Data(), buffer_->Capacity()),
                [this, self](boost::system::error_code ec, size_t length) {
                    if (ec) {
                        HandleDisconnect(ec);
                        return;
                    }
                    ProcessData(length);
                });
    }

    void ProcessData(size_t length) {
        buffer_->SetSize(length);

        try {
            uint32_t opcode = buffer_->ReadInt32();
            if (auto it = handlers_.find(opcode); it != handlers_.end()) {
                it->second(*buffer_);
            } else {
                std::cerr << "[SERVER] Unknown opcode: " << opcode << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[SERVER] Protocol error: " << e.what() << "\n";
            Close();
            return;
        }

        DoRead();
    }

    void HandleDisconnect(const boost::system::error_code& ec) {
        if (ec == boost::asio::error::eof) {
            std::cout << "[SERVER] Client disconnected\n";
        } else if (ec) {
            std::cerr << "[SERVER] Error: " << ec.message() << "\n";
        }
        Close();
    }

    void Close() {
        boost::system::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        if (ec) {
            std::cerr << "[SERVER] Shutdown error: " << ec.message() << "\n";
        }
        socket_.close(ec);
        if (ec) {
            std::cerr << "[SERVER] Close error: " << ec.message() << "\n";
        }
    }

    tcp::socket socket_;
    const std::unordered_map<uint32_t, AsyncServer::ClientHandler>& handlers_;
    std::unique_ptr<ByteBuffer> buffer_;
};

AsyncServer::AsyncServer(boost::asio::io_context& io_context, short port)
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    DoAccept();
}

void AsyncServer::Start() {
    std::cout << "[SERVER] Started on port " << acceptor_.local_endpoint().port() << "\n";
    io_context_.run();
}

void AsyncServer::SetHandler(uint32_t opcode, ClientHandler handler) {
    handlers_[opcode] = std::move(handler);
}

void AsyncServer::DoAccept() {
    acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::cout << "[SERVER] New connection\n";
                    std::make_shared<ClientSession>(std::move(socket), handlers_)->Start();
                } else {
                    std::cerr << "[SERVER] Accept error: " << ec.message() << "\n";
                }
                DoAccept(); // Важно: рекурсивный вызов
            });
}