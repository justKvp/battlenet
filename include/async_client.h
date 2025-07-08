#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <queue>
#include <functional>
#include <mutex>
#include <chrono>
#include "byte_buffer.h"
#include "opcodes.h"

#define CLIENT_LOG(msg) std::cout << "[CLIENT][" << __TIME__ << "] " << msg << "\n"
#define MAX_RECONNECT_ATTEMPTS 3
#define CONNECTION_TIMEOUT 5

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

class AsyncClient : public std::enable_shared_from_this<AsyncClient> {
public:
    AsyncClient(boost::asio::io_context& io_context,
                const tcp::resolver::results_type& endpoints);

    void Start(std::function<void()> on_connected = nullptr);
    void SendInventoryMove(uint8_t bag, uint8_t slot, uint32_t itemId);
    void SetResponseHandler(std::function<void(bool)> handler);
    void SetErrorHandler(std::function<void(const std::string&)> handler);
    void Disconnect();

private:
    void DoConnect();
    void DoReadHeader();
    void DoReadBody(uint16_t size);
    void DoWrite();
    void HandleResponse(bool success);
    void HandleError(const std::string& error_msg);
    void StartConnectionTimer();
    void Reconnect();

    static constexpr size_t HEADER_SIZE = sizeof(uint16_t);
    static constexpr size_t MAX_PACKET_SIZE = 4096;

    tcp::socket socket_;
    tcp::resolver::results_type endpoints_;
    std::unique_ptr<uint8_t[]> read_buffer_;
    std::queue<ByteBuffer> write_queue_;
    uint16_t current_opcode_;
    std::function<void(bool)> response_handler_;
    std::function<void(const std::string&)> error_handler_;
    std::function<void()> on_connected_;
    std::mutex socket_mutex_;
    boost::asio::steady_timer timer_;
    bool disconnected_ = false;
    bool is_connected_ = false;
    int reconnect_attempts_ = 0;
};