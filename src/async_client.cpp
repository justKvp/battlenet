#include "async_client.hpp"
#include <iostream>

void AsyncClient::connect(const std::string& host, const std::string& port) {
    std::cout << "[CLIENT] Connecting to " << host << ":" << port << "...\n";

    tcp::resolver resolver(socket_.get_executor());
    auto endpoints = resolver.resolve(host, port);

    connect_timer_.expires_after(std::chrono::seconds(10));
    connect_timer_.async_wait([this](auto ec) {
        if (!ec && !is_connected_) {
            std::cerr << "[CLIENT] Connection timeout\n";
            close();
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

        is_connected_ = true;
        std::cout << "[CLIENT] Connected to "
                  << socket_.remote_endpoint().address().to_string() << ":"
                  << socket_.remote_endpoint().port() << "\n";

        socket_.set_option(tcp::no_delay(true));
        reset_timeout();
        do_read_header();

        // Теперь можно отправлять данные из очереди
        if (!write_queue_.empty()) {
            do_write();
        }
    });
}

void AsyncClient::send(Opcode opcode, const ByteBuffer& body) {
    if (!is_connected_) {
        std::cout << "[CLIENT] Queuing packet (opcode: " << static_cast<int>(opcode)
                  << ") while connecting\n";
        write_queue_.emplace(opcode, body);
        return;
    }

    std::cout << "[CLIENT] Sending packet (opcode: " << static_cast<int>(opcode)
              << ", size: " << body.size() << " bytes)\n";

    write_queue_.emplace(opcode, body);
    if (!is_writing_) {
        do_write();
    }
}

void AsyncClient::do_write() {
    // Проверяем очередь
    if (write_queue_.empty()) {
        is_writing_ = false;
        return;
    }

    // Берем первый пакет из очереди
    is_writing_ = true;
    const auto& [current_opcode, body] = write_queue_.front(); // C++17 structured binding
    write_queue_.pop();

    // Формируем пакет
    ByteBuffer packet;
    packet.write_uint16(static_cast<uint16_t>(current_opcode));
    packet.write_uint32(static_cast<uint32_t>(body.size()));
    packet.write(body.data(), body.size());

    // Отправляем асинхронно
    auto self = shared_from_this();
    boost::asio::async_write(
            socket_,
            boost::asio::buffer(packet.data(), packet.size()),
            [this, self, opcode = current_opcode](auto ec, auto bytes) { // Захватываем opcode
                if (ec) {
                    std::cerr << "[CLIENT] Write failed for opcode "
                              << static_cast<int>(opcode) << ": "
                              << ec.message() << "\n";
                    close();
                    return;
                }

                // Успешная отправка
                std::cout << "[CLIENT] Successfully sent opcode "
                          << static_cast<int>(opcode)
                          << ", " << bytes << " bytes\n";

                reset_timeout();
                do_write(); // Обрабатываем следующий пакет
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
                        std::cerr << "[CLIENT] Body read failed for opcode "
                                  << static_cast<int>(opcode) << ": "
                                  << ec.message() << "\n";
                    }
                    close();
                    return;
                }

                try {
                    ByteBuffer body;
                    body.write(buffer->data(), bytes);

                    // Вызываем обработчик ответа
                    handle_response(static_cast<Opcode>(opcode), body);

                    reset_timeout();
                    do_read_header();
                } catch (const std::exception& e) {
                    std::cerr << "[CLIENT] Body processing error for opcode "
                              << static_cast<int>(opcode) << ": " << e.what() << "\n";
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

void AsyncClient::handle_response(Opcode opcode, const ByteBuffer& body) {
    try {
        switch (opcode) {
            case Opcode::SERVER_AUTH_INIT_RES: {
                uint8_t status = body.read_uint8();
                std::string message = body.read_string();
                uint32_t token = body.read_uint32();

                std::cout << "[CLIENT] SERVER_AUTH_INIT_RES: "
                          << (status ? "Success" : "Failed") << "\n"
                          << "Message: " << message << "\n"
                          << "Token: " << token << "\n";
                break;
            }
            case Opcode::SERVER_HELLO_ANSWER: {
                std::string message = body.read_string();

                std::cout << "[CLIENT] SERVER_HELLO_ANSWER: "
                          << "Message: " << message << "\n";
                break;
            }
            case Opcode::SERVER_INTERNAL_ERROR_RES: {
                uint8_t error_code = body.read_uint8();
                std::string error_msg = body.read_string();

                std::cerr << "[CLIENT] Error response ("
                          << static_cast<int>(error_code) << "): "
                          << error_msg << "\n";
                break;
            }

            default:
                std::cerr << "[CLIENT] Unknown response opcode: "
                          << static_cast<int>(opcode) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[CLIENT] Response processing error: " << e.what() << "\n";
    }
}