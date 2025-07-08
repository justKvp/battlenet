#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include "common.hpp"

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        read_header();
    }

private:
    void read_header() {
        auto self = shared_from_this();
        boost::asio::async_read(socket_,
                                boost::asio::buffer(&header_size_, sizeof(header_size_)),
                                [self](boost::system::error_code ec, std::size_t) {
                                    if (!ec) self->read_body();
                                });
    }

    void read_body() {
        auto self = shared_from_this();
        received_buffer_.resize(header_size_);
        boost::asio::async_read(socket_,
                                boost::asio::buffer(received_buffer_),
                                [self](boost::system::error_code ec, std::size_t) {
                                    if (!ec) self->process_message();
                                });
    }

    void process_message() {
        try {
            Message msg = MessageBuilder::deserialize(received_buffer_);
            std::cout << "\nReceived message (opcode: " << msg.opcode << ")\n";

            // Вывод всех полей входящего сообщения
            for (const auto& [name, value] : msg.fields) {
                std::cout << "  " << name << ": ";
                if (auto v = std::get_if<uint8_t>(&value)) {
                    std::cout << static_cast<int>(*v);
                }
                else if (auto v = std::get_if<uint16_t>(&value)) {
                    std::cout << *v;
                }
                else if (auto v = std::get_if<std::string>(&value)) {
                    std::cout << *v;
                }
                else if (auto v = std::get_if<std::vector<uint8_t>>(&value)) {
                    std::cout << "[binary, size=" << v->size() << "]";
                }
                std::cout << "\n";
            }

            send_response(msg);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            send_error("Processing error");
        }
    }

    void send_response(const Message& request) {
        Message response;
        response.opcode = opcodes::RESPONSE;

        if (request.opcode == opcodes::HELLO) {
            response.fields["status"] = static_cast<uint8_t>(1);
            response.fields["message"] = std::string("Hello accepted");
        }
        else if (request.opcode == opcodes::BINARY_DATA) {
            response.fields["status"] = static_cast<uint8_t>(2);
            auto& data = std::get<std::vector<uint8_t>>(request.fields.at("data"));
            response.fields["data_size"] = static_cast<uint32_t>(data.size());
        }

        send_message(response);
    }

    void send_error(const std::string& error) {
        Message response;
        response.opcode = opcodes::RESPONSE;
        response.fields["error"] = error;
        response.fields["error_code"] = static_cast<uint8_t>(255);
        send_message(response);
    }

    void send_message(const Message& msg) {
        auto buffer = MessageBuilder::serialize(msg);
        uint32_t size = static_cast<uint32_t>(buffer.size());

        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(&size, sizeof(size)));
        buffers.push_back(boost::asio::buffer(buffer));

        auto self = shared_from_this();
        boost::asio::async_write(socket_, buffers,
                                 [self](boost::system::error_code ec, std::size_t) {
                                     if (ec) self->socket_.close();
                                 });
    }

    tcp::socket socket_;
    std::vector<uint8_t> received_buffer_;
    uint32_t header_size_ = 0;
};

class Server {
public:
    Server(boost::asio::io_context& io_context, short port)
            : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket) {
                    if (!ec) {
                        std::make_shared<Session>(std::move(socket))->start();
                    }
                    do_accept();
                });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;
        Server s(io_context, std::atoi(argv[1]));
        std::cout << "Server started on port " << argv[1] << "\n";
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}