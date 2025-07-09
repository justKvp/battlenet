#include "async_client.hpp"
#include <iostream>
#include <iomanip>

void AsyncClient::connect(const std::string& host, const std::string& port) {
    std::cout << "[CLIENT] Connecting to " << host << ":" << port << "...\n";

    tcp::resolver resolver(socket_.get_executor());
    boost::system::error_code resolve_ec;
    auto endpoints = resolver.resolve(host, port, resolve_ec);

    if (resolve_ec) {
        std::cerr << "[CLIENT] Resolve failed: " << resolve_ec.message() << "\n";
        return;
    }

    if (endpoints.empty()) {
        std::cerr << "[CLIENT] No endpoints available\n";
        return;
    }

    connect_timer_.expires_after(std::chrono::seconds(10));
    connect_timer_.async_wait([this](auto ec) {
        if (!ec) {
            std::cerr << "[CLIENT] Connection timeout\n";
            socket_.close();
        }
    });

    do_connect(*endpoints.begin());
}

void AsyncClient::do_connect(const tcp::endpoint& endpoint) {
    auto self = shared_from_this();
    socket_.async_connect(endpoint, [this, self](auto ec) {
        connect_timer_.cancel();

        if (ec) {
            std::cerr << "[CLIENT] Connection failed: " << ec.message() << "\n";
            return;
        }

        std::cout << "[CLIENT] Connected to "
                  << socket_.remote_endpoint().address().to_string() << ":"
                  << socket_.remote_endpoint().port() << "\n";

        socket_.set_option(tcp::no_delay(true));
        reset_timeout();
        do_read_header();
    });
}

void AsyncClient::send(Opcode opcode, const ByteBuffer& body) {
    std::cout << "[CLIENT] Sending packet (opcode: " << static_cast<int>(opcode)
              << ", size: " << body.size() << " bytes)\n";

    write_queue_.emplace(opcode, body);
    if (!is_writing_) {
        do_write();
    }
}

void AsyncClient::do_write() {
    if (write_queue_.empty()) {
        is_writing_ = false;
        return;
    }

    is_writing_ = true;
    auto [opcode, body] = write_queue_.front();
    write_queue_.pop();

    ByteBuffer header;
    header.write<uint16_t>(static_cast<uint16_t>(opcode));
    header.write<uint32_t>(static_cast<uint32_t>(body.size()));

    ByteBuffer packet;
    packet.append(header);  // Сначала добавляем заголовок
    packet.append(body);    // Затем добавляем тело

    auto self = shared_from_this();
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(packet.data(), packet.size()),
            [this, self](auto ec, auto bytes) {
                if (ec) {
                    std::cerr << "[CLIENT] Write failed: " << ec.message() << "\n";
                    close();
                    return;
                }

                std::cout << "[CLIENT] Successfully sent " << bytes << " bytes\n";
                reset_timeout();
                do_write();
            });
}

void AsyncClient::reset_timeout() {
    timeout_timer_.expires_after(std::chrono::seconds(30));
    timeout_timer_.async_wait([self = shared_from_this()](auto ec) {
        if (!ec) {
            std::cerr << "[CLIENT] Connection timeout\n";
            self->close();
        }
    });
}

void AsyncClient::do_read_header() {
    auto buffer = std::make_shared<std::array<uint8_t, 6>>();

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(*buffer),
            [this, self, buffer](auto ec, auto bytes) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        std::cerr << "[CLIENT] Header read failed: " << ec.message() << "\n";
                    }
                    close();
                    return;
                }

                try {
                    uint16_t opcode = *reinterpret_cast<uint16_t*>(buffer->data());
                    uint32_t body_size = *reinterpret_cast<uint32_t*>(buffer->data() + 2);

                    // Исправленная проверка (1 MB вместо 10 MB)
                    if (body_size > 1024 * 1024) {
                        throw std::runtime_error("Packet size exceeds 1 MB limit");
                    }

                    reset_timeout();
                    do_read_body(opcode, body_size);
                } catch (const std::exception& e) {
                    std::cerr << "[CLIENT] Header processing error: " << e.what() << "\n";
                    close();
                }
            });
}

void AsyncClient::do_read_body(uint16_t opcode, uint32_t body_size) {
    auto buffer = std::make_shared<std::vector<uint8_t>>(body_size);

    auto self = shared_from_this();
    boost::asio::async_read(
            socket_,
            boost::asio::buffer(*buffer),
            [this, self, buffer, opcode](auto ec, auto bytes) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        std::cerr << "[CLIENT] Body read failed: " << ec.message() << "\n";
                    }
                    close();
                    return;
                }

                try {
                    ByteBuffer body;
                    body.write(buffer->data(), bytes);

                    std::cout << "[CLIENT] Received full packet (opcode: " << opcode
                              << ", size: " << bytes << " bytes)\n";

                    // Обработка пакета
                    switch (static_cast<Opcode>(opcode)) {
                        case Opcode::AUTH_RESPONSE: {
                            auto message = body.read_string();
                            std::cout << "[CLIENT] Auth response: " << message << "\n";
                            break;
                        }
                        case Opcode::DATA_RESPONSE: {
                            auto value = body.read<uint32_t>();
                            auto text = body.read_string();
                            std::cout << "[CLIENT] Data response: " << value
                                      << ", text: " << text << "\n";
                            break;
                        }
                        default:
                            std::cerr << "[CLIENT] Unknown opcode: " << opcode << "\n";
                    }

                    reset_timeout();
                    do_read_header();
                } catch (const std::exception& e) {
                    std::cerr << "[CLIENT] Body processing error: " << e.what() << "\n";
                    close();
                }
            });
}

void AsyncClient::close() {
    if (!socket_.is_open()) return;

    std::cout << "[CLIENT] Closing connection...\n";
    boost::system::error_code ec;

    connect_timer_.cancel();
    timeout_timer_.cancel();

    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    if (ec) {
        std::cerr << "[CLIENT] Close error: " << ec.message() << "\n";
    }
}