#ifndef ASYNC_CLIENT_H
#define ASYNC_CLIENT_H

#include <boost/asio.hpp>
#include "byte_buffer.h"
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

class AsyncClient {
public:
    AsyncClient(const std::string& host, const std::string& port);
    ~AsyncClient();

    void Connect();
    void Send(const ByteBuffer& data);
    bool IsConnected() const;
    void Reconnect();

    void SendInventoryMove(int from, int to, int item);

private:
    void StartWrite();
    void StartRead();
    void HandleTimeout();

    std::string host_;
    std::string port_;

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::resolver resolver_;
    std::thread worker_thread_;

    mutable std::mutex connection_mutex_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> timeout_occurred_{false};

    std::mutex queue_mutex_;
    std::queue<ByteBuffer> outgoing_queue_;
    std::atomic<bool> writing_{false};

    ByteBuffer read_buffer_;
};

#endif