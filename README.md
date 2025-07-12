# purity

## Required

sudo apt install libpq-dev libpqxx-dev pkg-config


## Compilation

mkdir build && cd build

cmake ..

make -j 4             (or another threads count)

## Пример использования асинхронного запроса в обработчике опкода

Код заворачивается в лямбду, после чего отправляется на обработку в отдельный вызываемый поток из thread pool в виде корутины

```
case Opcode::CMSG_DATABASE_ASYNC_EXAMPLE: {
            std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode)
                      << "] CMSG_DATABASE_ASYNC_EXAMPLE\n";
            uint64_t id = packet.buffer.read_uint64();

            // АСИНХРОННЫЙ ЗАПРОС
            auto self = shared_from_this();
            async_query([self, id]() -> boost::asio::awaitable<void> {
                try {

                    PreparedStatement stmt("LOGIN_SEL_ACCOUNT_BY_ID");
                    stmt.set_param(0, id);

                    auto user = co_await self->server_->db()->Async.execute<UserRow>(stmt);
                    if (user) {
                        std::cout << "[Server][Async] id: " << user->id
                                  << ", name: " << user->name << "\n";
                    }

                    Packet resp;
                    resp.opcode = Opcode::SMSG_DATABASE_ASYNC_EXAMPLE;
                    self->send_packet(resp);

                } catch (const std::exception &ex) {
                    std::cerr << "[Server] Async DB error: " << ex.what() << "\n";
                }
                co_return;
            });

            break;
        }
```

