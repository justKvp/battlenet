#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <deque>
#include <atomic>
#include <string>
#include <chrono>

#include "Packet.hpp"
#include "src/server/Server.hpp"
#include "SRP.hpp"

class Server;

enum class SessionState {
    CONNECTED,
    AUTH_INFO_RECEIVED,
    AUTH_CHECK_RECEIVED,
    LOGON_CHALLENGE_SENT,
    LOGON_PROOF_VERIFIED,
    LOGGED_IN
};

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket, std::shared_ptr<Server> server);

    void start();
    void close();
    void send_packet(const Packet &packet);

    template<typename Func>
    void async_query(Func &&func);

    template<typename Func>
    void blocking_query(Func &&func);

    void reset_ping_timer();

    // === SRP API ===
    void initSRP();
    SRP* srp() { return srp_.get(); }

    void loadSRPVerifier(const std::string &salt, const std::string &verifier);
    void generateServerEphemeral();
    void processClientPublic(const std::string &A_hex);
    bool verifySRPProof(const std::string &M1_hex);
    std::string getSRPSalt() const;
    std::string getSRPB() const;
    void generateFakeSRPChallenge();

    // === Auth ===
    void setUserName(const std::string &uname) { username_ = uname; }
    const std::string &getUserName() const { return username_; }
    void setAuthenticated(bool v) { is_authenticated_ = v; }
    bool isAuthenticated() const { return is_authenticated_; }

    void setExistInDB() { is_exist_in_db_ = true; }
    bool isExistInDB() const { return is_exist_in_db_; }

    bool isOpened() { return closed_; }
    std::shared_ptr<Server> server() const { return server_; }

    uint32_t getServerToken() const { return server_token_; }
    void setClientToken(uint32_t t) { client_token_ = t; }
    uint32_t getClientToken() const { return client_token_; }

    void set_state(SessionState s) { state_ = s; }
    SessionState state() const { return state_; }

private:
    void read_header();
    void read_body(std::size_t size);
    void handle_packet(Packet &packet);
    void do_write();

    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<Server> server_;

    std::vector<uint8_t> header_buffer_{4};
    std::vector<uint8_t> body_buffer_;

    std::deque<std::vector<uint8_t>> write_queue_;
    bool writing_ = false;
    std::atomic<bool> closed_{false};

    boost::asio::steady_timer ping_timer_;
    std::chrono::steady_clock::time_point last_ping_;

    std::string username_;
    uint32_t server_token_{0};
    uint32_t client_token_{0};
    bool is_authenticated_ = false;
    bool is_exist_in_db_ = false;
    std::unique_ptr<SRP> srp_;

    SessionState state_ = SessionState::CONNECTED;
};
