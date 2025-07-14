# BattleNet - emulator

## Required

sudo apt install libpq-dev libpqxx-dev pkg-config


## Compilation

mkdir build && cd build

cmake ..

make -j 4             (or another threads count)

## Пример использования асинхронного запроса в обработчике опкода

Код заворачивается в лямбду, после чего отправляется на обработку в отдельный вызываемый поток из thread pool в виде корутины.

Пример при обращении к бд, из обработчиков:

Асинхронный
```
        auto self = session; // Это уже shared_ptr
        session->async_query([self, username]() -> boost::asio::awaitable<void> {
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
            co_return;  // <- вот тут корутина должна возвращаться
        });
```

Синхронный 
```
        auto self = session; // Это уже shared_ptr
        blocking_query([self, id]() {
            auto a_log = Logger::get();
            try {
                PreparedStatement stmt("LOGIN_SEL_ACCOUNT_BY_ID");
                stmt.set_param(0, id);

                auto user = self->server_->db()->Sync.execute<UserRow>(stmt);
                if (user) {
                    a_log->debug("[client_session][Sync] id: {}, name {}", user->id, user->name);
                }

                Packet resp;
                resp.opcode = Opcode::SMSG_DATABASE_SYNC_EXAMPLE;
                self->send_packet(resp);

            } catch (const std::exception &ex) {
                std::cerr << "[Server] Sync DB error: " << ex.what() << "\n";
            }
        });
```

Таблица в бд 
```
CREATE TABLE accounts (
  id SERIAL PRIMARY KEY,
  username VARCHAR(50) UNIQUE NOT NULL,
  salt VARCHAR(64) NOT NULL,
  verifier VARCHAR(512) NOT NULL,
  email VARCHAR(100),
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```