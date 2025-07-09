#ifndef ASYNC_SERVER_H
#define ASYNC_SERVER_H

#include <boost/asio.hpp>
#include "byte_buffer.h"
#include <functional>
#include <unordered_map>
#include <memory>

using boost::asio::ip::tcp;

class AsyncServer {
public:
    using ClientHandler = std::function<void(ByteBuffer&)>;

    AsyncServer(boost::asio::io_context& io_context, short port);
    void Start();
    void SetHandler(uint32_t opcode, ClientHandler handler);

private:
    class ClientSession;
    void DoAccept();

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::unordered_map<uint32_t, ClientHandler> handlers_;
};

#endif