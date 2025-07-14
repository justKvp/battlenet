#include "ClientSession.hpp"
#include "src/server/handlers/Handlers.hpp"
#include "Logger.hpp"
#include "spdlog/fmt/ranges.h"

using boost::asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket, std::shared_ptr<Server> server)
        : socket_(std::move(socket)),
          server_(std::move(server)),
          ping_timer_(socket_.get_executor()) {}

void ClientSession::start() {
    auto ep = socket_.remote_endpoint();
    Logger::get()->info("[client_session] New connection from {}:{}",
                        ep.address().to_string(), ep.port());

    server_token_ = static_cast<uint32_t>(std::rand());

    Packet p;
    p.opcode = SID_AUTH_INFO;
    p.buffer.write_uint32(0x49583836); // IX86
    p.buffer.write_uint32(0x57515233); // W3XP
    p.buffer.write_uint32(17085);      // Warcraft III 1.27.17085
    p.buffer.write_uint32(0);          // EXE hash
    p.buffer.write_uint32(server_token_);
    p.buffer.write_uint32(0);          // client_token
    p.buffer.write_string("PvPGN Banner");

    Logger::get()->debug("[start] Preparing SID_AUTH_INFO opcode={}", p.opcode);
    send_packet(p);
    Logger::get()->info("[client_session] Sent SID_AUTH_INFO");

    reset_ping_timer();
    read_header();
}

void ClientSession::close() {
    if (closed_.exchange(true)) return;

    ping_timer_.cancel();

    boost::system::error_code ec;
    if (socket_.is_open()) {
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    write_queue_.clear();
    srp_.reset();

    if (server_) {
        server_->remove_session(shared_from_this());
    }

    Logger::get()->info("[client_session] Closed");
}

template<typename Func>
void ClientSession::async_query(Func &&func) {
    auto self = shared_from_this();
    boost::asio::co_spawn(
            socket_.get_executor(),
            std::forward<Func>(func),
            boost::asio::detached
    );
}

template<typename Func>
void ClientSession::blocking_query(Func &&func) {
    auto self = shared_from_this();
    boost::asio::post(
            server_->thread_pool(),
            [self, func = std::forward<Func>(func)] {
                func();
            }
    );
}

void ClientSession::read_header() {
    auto self = shared_from_this();

    // Обнуляем header_buffer_ — очень важно!
    header_buffer_.fill(0);

    // Читаем ровно 4 байта BNCS header
    boost::asio::async_read(socket_, boost::asio::buffer(header_buffer_),
                            [this, self](boost::system::error_code ec, size_t bytes_transferred) {
                                auto log = Logger::get();

                                if (ec) {
                                    if (ec == boost::asio::error::eof) {
                                        log->info("[client_session] Client closed connection normally (EOF)");
                                    } else {
                                        log->error("[client_session] read_header failed: {}", ec.message());
                                    }
                                    close();
                                    return;
                                }

                                if (bytes_transferred != 4) {
                                    log->error("[client_session] read_header: expected 4 bytes, got {}", bytes_transferred);
                                    close();
                                    return;
                                }

                                uint32_t len = header_buffer_[0]
                                               | (header_buffer_[1] << 8)
                                               | (header_buffer_[2] << 16)
                                               | (header_buffer_[3] << 24);

                                log->debug("[read_header] Parsed BNCS length: {}", len);
                                log->debug("[read_header] Header raw: {:02X} {:02X} {:02X} {:02X}",
                                           header_buffer_[0], header_buffer_[1],
                                           header_buffer_[2], header_buffer_[3]);

                                if (len == 0) {
                                    log->error("[client_session] read_header: length is zero!");
                                    close();
                                    return;
                                }

                                body_buffer_.clear();
                                body_buffer_.resize(len);

                                read_body(len);
                            }
    );
}

void ClientSession::read_body(std::size_t size) {
    auto self = shared_from_this();

    boost::asio::async_read(socket_, boost::asio::buffer(body_buffer_),
                            [this, self, size](boost::system::error_code ec, size_t bytes_transferred) {
                                auto log = Logger::get();

                                if (ec) {
                                    if (ec == boost::asio::error::eof) {
                                        log->info("[client_session] Client closed connection normally (EOF)");
                                    } else {
                                        log->error("[client_session] read_body failed: {}", ec.message());
                                    }
                                    close();
                                    return;
                                }

                                if (bytes_transferred != size) {
                                    log->error("[client_session] read_body: expected {} bytes, got {}", size, bytes_transferred);
                                    close();
                                    return;
                                }

                                // Для отладки — полный дамп
                                std::string hex;
                                for (uint8_t b : body_buffer_)
                                    hex += fmt::format("{:02X} ", b);
                                log->debug("[read_body] RAW: {}", hex);

                                try {
                                    Packet pkt = Packet::deserialize(body_buffer_);
                                    log->debug("[read_body] OPCODE: {}", static_cast<int>(pkt.opcode));

                                    handle_packet(pkt);

                                    // Продолжить цикл
                                    read_header();
                                } catch (const std::exception &ex) {
                                    log->error("[client_session] Packet deserialization failed: {}", ex.what());
                                    close();
                                }
                            }
    );
}

void ClientSession::handle_packet(Packet &packet) {
    Handlers::dispatch(shared_from_this(), packet);
}

void ClientSession::send_packet(const Packet &packet) {
    auto serialized = packet.serialize();

    bool idle = write_queue_.empty();
    write_queue_.push_back(std::move(serialized));

    if (!writing_ && idle) {
        do_write();
    }
}

void ClientSession::do_write() {
    if (write_queue_.empty()) {
        writing_ = false;
        return;
    }

    writing_ = true;
    auto self = shared_from_this();

    // 🟢 Вот дамп — прямо перед write
    Logger::get()->debug("[do_write] RAW OUT: {}", fmt::join(write_queue_.front(), " "));

    boost::asio::async_write(socket_, boost::asio::buffer(write_queue_.front()),
                             [this, self](boost::system::error_code ec, std::size_t) {
                                 auto log = Logger::get();
                                 if (ec) {
                                     if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::eof) {
                                         log->info("[client_session] [do_write] Client disconnected");
                                     } else {
                                         log->error("[client_session] [do_write] Write failed: {}", ec.message());
                                     }
                                     close();
                                     return;
                                 }

                                 write_queue_.pop_front();
                                 do_write();
                             });
}

void ClientSession::reset_ping_timer() {
    auto self = shared_from_this();
    last_ping_ = std::chrono::steady_clock::now();

    ping_timer_.expires_after(std::chrono::seconds(60));
    ping_timer_.async_wait([this, self](const boost::system::error_code &ec) {
        if (ec == boost::asio::error::operation_aborted) return;
        if (!ec) {
            Logger::get()->warn("[client_session] Ping timeout!");
            close();
        }
    });
}

void ClientSession::initSRP() {
    if (!srp_) srp_ = std::make_unique<SRP>();
}

void ClientSession::loadSRPVerifier(const std::string &salt, const std::string &verifier) {
    if (!srp_) throw std::runtime_error("SRP not initialized");
    srp_->load_verifier(salt, verifier);
}

void ClientSession::generateServerEphemeral() {
    if (!srp_) throw std::runtime_error("SRP not initialized");
    srp_->generate_server_ephemeral();
}

void ClientSession::processClientPublic(const std::string &A_hex) {
    if (!srp_) throw std::runtime_error("SRP not initialized");
    srp_->process_client_public(A_hex);
}

bool ClientSession::verifySRPProof(const std::string &M1_hex) {
    if (!srp_) throw std::runtime_error("SRP not initialized");
    return srp_->verify_proof(M1_hex);
}

std::string ClientSession::getSRPSalt() const {
    if (!srp_) throw std::runtime_error("SRP not initialized");
    return srp_->get_salt();
}

std::string ClientSession::getSRPB() const {
    if (!srp_) throw std::runtime_error("SRP not initialized");
    return srp_->get_B();
}

void ClientSession::generateFakeSRPChallenge() {
    if (!srp_) throw std::runtime_error("SRP not initialized");
    srp_->generate_fake_challenge();
}
