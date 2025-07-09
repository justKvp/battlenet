// ServerWithHeartbeat.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <memory>
#include "Packet.hpp"

using boost::asio::ip::tcp;
using namespace std::chrono;

class Session : public std::enable_shared_from_this<Session> {
    tcp::socket socket;
    std::vector<uint8_t> read_buffer;
    boost::asio::steady_timer timeout_timer;
    bool connected = true;
    const seconds timeout_interval = seconds(15);

public:
    Session(boost::asio::io_context& io, tcp::socket sock)
            : socket(std::move(sock)), timeout_timer(io) {}

    void start() {
        std::cout << "[Server] New session\n";
        reset_timeout();
        do_read();
    }

private:
    void reset_timeout() {
        auto self = shared_from_this();
        timeout_timer.expires_after(timeout_interval);
        timeout_timer.async_wait([this, self](boost::system::error_code ec) {
            if (!ec && connected) {
                std::cout << "[Server] Client timed out. Closing connection.\n";
                boost::system::error_code ignored;
                socket.shutdown(tcp::socket::shutdown_both, ignored);
                socket.close();
                connected = false;
            }
        });
    }

    void do_read() {
        auto self = shared_from_this();
        read_buffer.resize(1024);
        socket.async_read_some(boost::asio::buffer(read_buffer),
                               [this, self](boost::system::error_code ec, std::size_t length) {
                                   if (ec) {
                                       connected = false;
                                       std::cerr << "[Server] Read error: " << ec.message() << '\n';
                                       return;
                                   }

                                   try {
                                       Packet packet = Packet::deserialize({read_buffer.begin(), read_buffer.begin() + length});
                                       handle_packet(packet);
                                       do_read(); // продолжить чтение
                                   } catch (const std::exception& e) {
                                       std::cerr << "[Server] Packet error: " << e.what() << '\n';
                                   }
                               });
    }

    void handle_packet(Packet& packet) {
        if (!connected) return;

        switch (packet.opcode) {
            case Opcode::PING:
                std::cout << "[Server] Received PING\n";
                reset_timeout(); // обновить таймер
                break;
            case Opcode::MESSAGE: {
                std::string msg = packet.buffer.read_string();
                std::cout << "[Server] Message: " << msg << "\n";
                break;
            }
            default:
                std::cerr << "[Server] Unknown opcode\n";
        }
    }
};

class Server {
    boost::asio::io_context& io_context;
    tcp::acceptor acceptor;

public:
    Server(boost::asio::io_context& io, short port)
            : io_context(io), acceptor(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec)
                std::make_shared<Session>(io_context, std::move(socket))->start();
            do_accept();
        });
    }
};

int main() {
    try {
        boost::asio::io_context io;
        Server server(io, 5555);
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << '\n';
    }

    return 0;
}