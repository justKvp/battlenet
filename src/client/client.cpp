#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include "common.hpp"

using boost::asio::ip::tcp;

class Client : public std::enable_shared_from_this<Client> {
public:
    // Публичный конструктор
    Client(boost::asio::io_context& io_context,
           const std::string& host,
           const std::string& port)
            : socket_(io_context),  // Используем io_context только для инициализации сокета
              resolver_(io_context),
              host_(host),
              port_(port) {}

    // Фабричный метод
    static std::shared_ptr<Client> create(boost::asio::io_context& io_context,
                                          const std::string& host,
                                          const std::string& port) {
        auto ptr = std::shared_ptr<Client>(new Client(io_context, host, port));
        ptr->start();
        return ptr;
    }

    void send_hello() {
        Message msg;
        msg.opcode = opcodes::HELLO;
        msg.fields["protocol"] = static_cast<uint8_t>(2);
        msg.fields["client_id"] = static_cast<uint32_t>(123456);
        msg.fields["flags"] = static_cast<uint16_t>(0x00FF);
        msg.fields["app_name"] = std::string("TestClient");
        send_message(msg);
    }

    void close() {
        if (socket_.is_open()) {
            boost::system::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
    }

private:
    void start() {
        do_connect();
    }

    void do_connect() {
        auto self = shared_from_this();
        resolver_.async_resolve(host_, port_,
                                [self](boost::system::error_code ec, tcp::resolver::results_type results) {
                                    if (!ec) {
                                        boost::asio::async_connect(self->socket_, results,
                                                                   [self](boost::system::error_code ec, tcp::endpoint) {
                                                                       if (!ec) self->read_header();
                                                                   });
                                    }
                                });
    }

    void read_header() {
        auto self = shared_from_this();
        boost::asio::async_read(socket_,
                                boost::asio::buffer(&header_size_, sizeof(header_size_)),
                                [self](boost::system::error_code ec, std::size_t) {
                                    if (!ec && self->header_size_ <= 10*1024*1024) {
                                        self->read_body();
                                    }
                                });
    }

    void read_body() {
        auto self = shared_from_this();
        received_buffer_.resize(header_size_);
        boost::asio::async_read(socket_,
                                boost::asio::buffer(received_buffer_),
                                [self](boost::system::error_code ec, std::size_t) {
                                    if (!ec) self->process_response();
                                });
    }

    void process_response() {
        try {
            Message msg = MessageBuilder::deserialize(received_buffer_);
            std::cout << "\nServer response:\n";
            std::cout << "  Opcode: " << msg.opcode << "\n";

            for (const auto& [name, value] : msg.fields) {
                std::cout << "  " << name << ": ";

                if (auto v = std::get_if<uint8_t>(&value)) {
                    std::cout << static_cast<int>(*v);
                }
                else if (auto v = std::get_if<uint16_t>(&value)) {
                    std::cout << *v;
                }
                else if (auto v = std::get_if<uint32_t>(&value)) {
                    std::cout << *v;
                }
                else if (auto v = std::get_if<uint64_t>(&value)) {
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
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
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
                                     if (ec) self->close();
                                 });
    }

    tcp::socket socket_;
    tcp::resolver resolver_;
    std::string host_;
    std::string port_;
    std::vector<uint8_t> received_buffer_;
    uint32_t header_size_ = 0;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;
        auto client = Client::create(io_context, argv[1], argv[2]);
        std::thread t([&io_context](){ io_context.run(); });

        std::this_thread::sleep_for(std::chrono::seconds(1));
        client->send_hello();

        std::this_thread::sleep_for(std::chrono::seconds(2));
        client->close();
        t.join();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}