#include "async_client.hpp"
#include <iostream>

void AsyncClient::connect(const std::string& host, const std::string& port) {
    tcp::resolver resolver(socket_.get_executor());

    boost::system::error_code resolve_ec;
    auto endpoints = resolver.resolve(host, port, resolve_ec);

    if (resolve_ec) {
        std::cerr << "Resolve error: " << resolve_ec.message() << "\n";
        return;
    }

    if (endpoints.empty()) {
        std::cerr << "No endpoints found\n";
        return;
    }

    connect_timer_.expires_after(std::chrono::seconds(10));
    connect_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) socket_.close();
    });

    auto self = shared_from_this();
    boost::asio::async_connect(
            socket_,
            endpoints,
            [this, self](const boost::system::error_code& ec,
                         const tcp::endpoint& selected_endpoint) {
                connect_timer_.cancel();
                if (ec) {
                    std::cerr << "Connect error: " << ec.message() << "\n";
                    return;
                }

                std::cout << "Connected to: "
                          << selected_endpoint.address().to_string()
                          << ":" << selected_endpoint.port() << "\n";

                socket_.set_option(tcp::no_delay(true));
                reset_timeout();
                do_read_header();
            });
}

void AsyncClient::send(Opcode opcode, const ByteBuffer& body) {
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

    ByteBuffer packet;
    packet.write<uint16_t>(static_cast<uint16_t>(opcode));
    packet.write<uint32_t>(static_cast<uint32_t>(body.size()));
    packet.write(body.data(), body.size());

    auto self = shared_from_this();
    boost::asio::async_write(socket_,
                             boost::asio::buffer(packet.data(), packet.size()),
                             [this, self](auto ec, auto) {
                                 if (ec) {
                                     std::cerr << "Write error: " << ec.message() << "\n";
                                     close();
                                     return;
                                 }

                                 reset_timeout();
                                 do_write();
                             });
}

void AsyncClient::reset_timeout() {
    timeout_timer_.expires_after(std::chrono::seconds(30));
    timeout_timer_.async_wait([self = shared_from_this()](auto ec) {
        if (!ec) self->close();
    });
}

void AsyncClient::do_read_header() {
    auto buffer = std::make_shared<std::array<uint8_t, 6>>();

    auto self = shared_from_this();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(*buffer),
                            [this, self, buffer](auto ec, auto bytes) {
                                if (ec) {
                                    if (ec != boost::asio::error::operation_aborted) {
                                        std::cerr << "Read header error: " << ec.message() << "\n";
                                    }
                                    close();
                                    return;
                                }

                                try {
                                    read_buffer_.clear();
                                    read_buffer_.write(buffer->data(), bytes);

                                    uint16_t opcode = read_buffer_.read<uint16_t>();
                                    uint32_t body_size = read_buffer_.read<uint32_t>();

                                    if (body_size > 1024*1024) {
                                        throw std::runtime_error("Packet too large");
                                    }

                                    reset_timeout();
                                    do_read_body(opcode, body_size);
                                } catch (const std::exception& e) {
                                    std::cerr << "Header processing error: " << e.what() << "\n";
                                    close();
                                }
                            });
}

void AsyncClient::do_read_body(uint16_t opcode, uint32_t body_size) {
    auto buffer = std::make_shared<std::vector<uint8_t>>(body_size);

    auto self = shared_from_this();
    boost::asio::async_read(socket_,
                            boost::asio::buffer(*buffer),
                            [this, self, buffer, opcode](auto ec, auto bytes) {
                                if (ec) {
                                    std::cerr << "Read body error: " << ec.message() << "\n";
                                    close();
                                    return;
                                }

                                try {
                                    read_buffer_.clear();
                                    read_buffer_.write(buffer->data(), bytes);

                                    std::cout << "Received packet, opcode: " << opcode
                                              << ", size: " << bytes << " bytes\n";

                                    reset_timeout();
                                    do_read_header();
                                } catch (const std::exception& e) {
                                    std::cerr << "Body processing error: " << e.what() << "\n";
                                    close();
                                }
                            });
}

void AsyncClient::close() {
    boost::system::error_code ec;
    socket_.close(ec);
    connect_timer_.cancel();
    timeout_timer_.cancel();
}