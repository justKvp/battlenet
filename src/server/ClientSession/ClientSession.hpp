#pragma once

#include "Packet.hpp"
#include "ByteBuffer.hpp"
#include "Logger.hpp"
#include "SRP.hpp"
#include "src/server/Server.hpp"

#include <boost/asio.hpp>
#include <deque>
#include <memory>
#include <array>
#include <chrono>

enum class SessionState {
    CONNECTED,           // После TCP connect и отправки SID_AUTH_INFO
    BNCS_PING,           // После получения первого BNCS_PING
    AUTH_CHECK_SENT,     // После того как сервер послал SID_AUTH_CHECK
    AUTH_CHECK_RECEIVED, // После того как клиент вернул SID_AUTH_CHECK
    AUTH_INFO_RECEIVED,  // После того как обработали SID_AUTH_INFO
    LOGGED_IN,           // После успешного PROOF
    CLOSED
};

class Server;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::ip::tcp::socket socket, std::shared_ptr<Server> server);

    void start();
    void close();

    void send_packet(const Packet &packet);

    void reset_ping_timer();

    std::shared_ptr<Server> server() const { return server_; }

    // === SRP related ===
    void initSRP();
    void loadSRPVerifier(const std::string &salt, const std::string &verifier);
    void generateServerEphemeral();
    void processClientPublic(const std::string &A_hex);
    bool verifySRPProof(const std::string &M1_hex);
    std::string getSRPSalt() const;
    std::string getSRPB() const;
    void generateFakeSRPChallenge();

    void setUserName(const std::string &name) { user_name_ = name; }
    std::string getUserName() const { return user_name_; }

    SessionState state() const { return state_; }
    void set_state(SessionState s) { state_ = s; }

    bool isExistInDB() const { return exist_in_db_; }
    void setExistInDB(bool val = true) { exist_in_db_ = val; }

    bool isAuthenticated() const { return authenticated_; }
    void setAuthenticated(bool val = true) { authenticated_ = val; }
    uint32_t getServerToken() const { return server_token_; }
    uint32_t getClientToken() const { return client_token_; }
    void setClientToken(uint32_t token) { client_token_ = token; }

    bool isOpened() { return closed_; }

    SRP* srp() { return srp_.get(); }

private:
    void read_loop();
    void handle_packet(Packet &packet);
    void do_write();

    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<Server> server_;

    std::deque<std::vector<uint8_t>> write_queue_;
    bool writing_ = false;

    std::vector<uint8_t> read_buffer_;
    std::vector<uint8_t> inbuf_;

    uint32_t server_token_{};
    uint32_t client_token_{};

    std::unique_ptr<SRP> srp_;
    std::chrono::steady_clock::time_point last_ping_;
    boost::asio::steady_timer ping_timer_;
    std::atomic<bool> closed_{false};

    std::string user_name_;
    SessionState state_ = SessionState::CONNECTED;
    bool exist_in_db_ = false;
    bool authenticated_ = false;
};
