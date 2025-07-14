#include "Handlers.hpp"
#include "Logger.hpp"
#include "opcodes.hpp"
#include "AuthProofCode.hpp"

#include <algorithm>

using namespace Handlers;

void Handlers::dispatch(std::shared_ptr<ClientSession> session, Packet &p) {
    switch (p.opcode) {
        case Opcode::SID_BNCS_PING:
            if (session->state() != SessionState::CONNECTED) {
                Logger::get()->warn("Unexpected SID_BNCS_PING in state {}", static_cast<int>(session->state()));
                session->close();
                return;
            }
            session->set_state(SessionState::BNCS_PING);
            handle_bncs_ping(session, p);
            break;

        case Opcode::SID_AUTH_CHECK:
            if (session->state() != SessionState::AUTH_CHECK_SENT) {
                Logger::get()->warn("Unexpected SID_AUTH_CHECK in state {}", static_cast<int>(session->state()));
                session->close();
                return;
            }
            Handlers::handle_auth_check(session, p);
            session->set_state(SessionState::AUTH_CHECK_RECEIVED);
            break;

        case Opcode::SID_AUTH_INFO:
            if (session->state() != SessionState::AUTH_CHECK_RECEIVED) {
                Logger::get()->warn("Unexpected SID_AUTH_INFO in state {}", static_cast<int>(session->state()));
                session->close();
                return;
            }
            boost::asio::co_spawn(
                    session->server()->thread_pool(),
                    handle_auth_info(session, p),
                    boost::asio::detached
            );
            break;

        case Opcode::SID_LOGON_PROOF:
            if (session->state() != SessionState::AUTH_INFO_RECEIVED) {
                Logger::get()->warn("Unexpected SID_LOGON_PROOF in state {}", static_cast<int>(session->state()));
                session->close();
                return;
            }
            boost::asio::co_spawn(
                    session->server()->thread_pool(),
                    handle_logon_proof(session, p),
                    boost::asio::detached
            );
            break;

        case Opcode::SID_ENTERCHAT:
            handle_enterchat(session, p);
            break;

        case Opcode::SID_CHATCOMMAND:
            handle_chat_command(session, p);
            break;

        default:
            Logger::get()->warn("[Handlers] Unknown opcode: {}", static_cast<int>(p.opcode));
            break;
    }
}

void Handlers::handle_ping(std::shared_ptr<ClientSession> session, Packet &p) {
    uint32_t ping_value = p.buffer.read_uint32();
    Logger::get()->info("[Handlers] SID_PING: {}", ping_value);

    session->reset_ping_timer();

    Packet reply;
    reply.opcode = Opcode::SID_PING;
    reply.buffer.write_uint32(ping_value);
    session->send_packet(reply);
}

void Handlers::handle_bncs_ping(std::shared_ptr<ClientSession> session, Packet &p) {
    Logger::get()->info("[Handlers] SID_BNCS_PING");

    session->reset_ping_timer();

    // Ответить PING
    Packet reply;
    reply.opcode = Opcode::SID_BNCS_PING;
    session->set_state(SessionState::AUTH_CHECK_SENT);
    session->send_packet(reply);

    // Отправить SID_AUTH_CHECK
    Packet auth_check;
    auth_check.opcode = Opcode::SID_AUTH_CHECK;
    auth_check.buffer.write_uint32(session->getServerToken());
    auth_check.buffer.write_uint32(17085);
    auth_check.buffer.write_uint32(0);
    auth_check.buffer.write_string("");
    session->send_packet(auth_check);
}

void Handlers::handle_auth_check(std::shared_ptr<ClientSession> session, Packet &p) {
    auto log = Logger::get();
    log->info("[Handlers] SID_AUTH_CHECK received");

    uint32_t client_token = p.buffer.read_uint32();
    uint32_t exe_version = p.buffer.read_uint32();
    uint32_t exe_hash = p.buffer.read_uint32();
    std::string cdkey_owner = p.buffer.read_string();

    session->setClientToken(client_token);

    log->debug("Tokens: server_token={}, client_token={}", session->getServerToken(), client_token);

    Packet reply;
    reply.opcode = Opcode::SID_AUTH_CHECK;
    reply.buffer.write_uint32(client_token);
    reply.buffer.write_uint32(exe_version);
    reply.buffer.write_uint32(exe_hash);
    reply.buffer.write_uint32(0); // key_status OK
    reply.buffer.write_uint32(0); // account_status OK
    session->set_state(SessionState::AUTH_CHECK_RECEIVED);
    session->send_packet(reply);
}

boost::asio::awaitable<void> Handlers::handle_auth_info(std::shared_ptr<ClientSession> session, Packet &p) {
    auto log = Logger::get();

    uint32_t client_token = p.buffer.read_uint32();
    uint32_t exe_version = p.buffer.read_uint32();
    uint32_t exe_hash = p.buffer.read_uint32();
    std::string origin_username = p.buffer.read_string();

    log->info("[Handlers] SID_AUTH_INFO: user='{}' token={} ver={} hash={}",
              origin_username, client_token, exe_version, exe_hash);

    session->setUserName(origin_username);
    std::string lower_name = origin_username;

    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    PreparedStatement stmt("SELECT_ACCOUNT_BY_USERNAME");
    stmt.set_param(0, lower_name);

    auto user = co_await session->server()->db()->Async.execute<AccountSaltAndVerifierRow>(stmt);

    session->initSRP();

    if (user) {
        log->info("[Handlers] User '{}' found.", lower_name);
        session->loadSRPVerifier(user->salt, user->verifier);
        session->generateServerEphemeral();
        session->setExistInDB();
    } else {
        log->info("[Handlers] User '{}' not found, using fake challenge.", lower_name);
        session->generateFakeSRPChallenge();
    }

    Packet reply;
    reply.opcode = Opcode::SID_LOGON_CHALLENGE;
    reply.buffer.write_string(session->getSRPSalt());
    reply.buffer.write_string(session->getSRPB());
    session->set_state(SessionState::AUTH_INFO_RECEIVED);
    session->send_packet(reply);
    co_return;
}

boost::asio::awaitable<void> Handlers::handle_logon_proof(std::shared_ptr<ClientSession> session, Packet &p) {
    auto log = Logger::get();

    std::string A_hex = p.buffer.read_string();
    std::string M1_hex = p.buffer.read_string();

    log->debug("[Handlers] SID_LOGON_PROOF: A={}, M1={}", A_hex, M1_hex);

    session->processClientPublic(A_hex);

    std::string lower_username = session->getUserName();
    std::transform(lower_username.begin(), lower_username.end(), lower_username.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (session->isExistInDB()) {
        log->debug("[Handlers] Account '{}' exists.", lower_username);
    } else {
        log->debug("[Handlers] Account '{}' new. Auto-registering.", lower_username);

        std::string salt_hex, verifier_hex;
        session->srp()->generate_verifier_from_proof(A_hex, M1_hex, salt_hex, verifier_hex);

        PreparedStatement stmt("INSERT_ACCOUNT_BY_USERNAME");
        stmt.set_param(0, lower_username);
        stmt.set_param(1, salt_hex);
        stmt.set_param(2, verifier_hex);

        auto new_id = co_await session->server()->db()->Async.execute<int64_t>(stmt);
        if (new_id) {
            log->info("[Handlers] Inserted account '{}', id={}", lower_username, *new_id);
        }

        session->loadSRPVerifier(salt_hex, verifier_hex);
        session->setExistInDB();
    }

    bool proof_ok = session->verifySRPProof(M1_hex);
    if (!proof_ok) {
        log->warn("[Handlers] SRP proof failed for '{}'. Closing.", lower_username);

        Packet fail;
        fail.opcode = Opcode::SID_LOGON_PROOF;
        fail.buffer.write_uint8(static_cast<uint8_t>(AuthProofCode::FAIL));
        session->send_packet(fail);

        session->close();
        co_return;
    }

    session->setAuthenticated(true);
    log->info("[Handlers] User '{}' authenticated OK.", lower_username);

    Packet reply;
    reply.opcode = Opcode::SID_LOGON_PROOF;
    reply.buffer.write_uint8(static_cast<uint8_t>(AuthProofCode::SUCCESS));
    session->set_state(SessionState::LOGGED_IN);
    session->send_packet(reply);
    co_return;
}

void Handlers::handle_enterchat(std::shared_ptr<ClientSession> session, Packet &p) {
    auto log = Logger::get();

    std::string account_name = p.buffer.read_string();
    std::string channel_name = p.buffer.read_string();

    log->info("[Handlers] SID_ENTERCHAT: user='{}' channel='{}'", account_name, channel_name);

    // Example: send success reply
    Packet reply;
    reply.opcode = Opcode::SID_ENTERCHAT;
    reply.buffer.write_string(account_name);
    session->send_packet(reply);
}

void Handlers::handle_chat_command(std::shared_ptr<ClientSession> session, Packet &p) {
    auto log = Logger::get();

    std::string command = p.buffer.read_string();
    log->info("[Handlers] SID_CHATCOMMAND: {}", command);

    // TODO: parse /who, /join, etc.
}
