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
    p.buffer.write_uint32(17085);      // Version
    p.buffer.write_uint32(0);          // exe hash
    p.buffer.write_uint32(server_token_);
    p.buffer.write_uint32(0);          // client_token
    p.buffer.write_string("PvPGN Banner");

    Logger::get()->debug("[start] Buffer size: {}", p.buffer.size());
    Logger::get()->debug("[Packet::serialize] BNCS len: {}", p.serialize().size());

    send_packet(p);
    Logger::get()->info("[client_session] Sent SID_AUTH_INFO");

    reset_ping_timer();
    read_loop();
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

void ClientSession::read_loop() {
    auto self = shared_from_this();
    read_buffer_.resize(4096);

    socket_.async_read_some(boost::asio::buffer(read_buffer_),
                            [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                                auto log = Logger::get();
                                if (ec) {
                                    if (ec == boost::asio::error::eof) {
                                        log->info("[client_session] [read_loop] Client closed connection normally (EOF)");
                                    } else {
                                        log->info("[client_session] [read_loop] read_loop closed: {}", ec.message());
                                    }
                                    close();
                                    return;
                                }

                                inbuf_.insert(inbuf_.end(), read_buffer_.begin(), read_buffer_.begin() + bytes_transferred);

                                while (inbuf_.size() >= 4) {
                                    uint32_t len =
                                            (inbuf_[0]) |
                                            (inbuf_[1] << 8) |
                                            (inbuf_[2] << 16) |
                                            (inbuf_[3] << 24);

                                    if (inbuf_.size() < 4 + len) break;

                                    std::vector<uint8_t> packet_data(inbuf_.begin() + 4, inbuf_.begin() + 4 + len);

                                    Packet pkt = Packet::deserialize(packet_data);
                                    handle_packet(pkt);

                                    inbuf_.erase(inbuf_.begin(), inbuf_.begin() + 4 + len);
                                }

                                read_loop();
                            });
}

void ClientSession::handle_packet(Packet &packet) {
    Handlers::dispatch(shared_from_this(), packet);
}

void ClientSession::send_packet(const Packet &packet) {
    auto raw = packet.serialize();
    Logger::get()->debug("[send_packet] RAW: {}", fmt::join(raw, " "));

    bool idle = write_queue_.empty();
    write_queue_.push_back(raw);

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

    boost::asio::async_write(socket_, boost::asio::buffer(write_queue_.front()),
                             [this, self](boost::system::error_code ec, std::size_t) {
                                 if (ec) {
                                     if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::eof) {
                                         Logger::get()->info("[client_session] [do_write] Client disconnected");
                                     } else {
                                         Logger::get()->error("[client_session] [do_write] failed: {}", ec.message());
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
