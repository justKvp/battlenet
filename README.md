# purity

## Required

sudo apt install libpq-dev libpqxx-dev pkg-config


## Compilation

mkdir build && cd build

cmake ..

make -j 4             (or another threads count)



case Opcode::CMSG_DATABASE_ASYNC_EXAMPLE: {
std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode)
<< "] CMSG_DATABASE_ASYNC_EXAMPLE\n";

            // АСИНХРОННЫЙ ЗАПРОС
            auto self = shared_from_this();
            spawn([self]() -> boost::asio::awaitable<void> {
                try {
                    PreparedStatement stmt("LOGIN_SEL_ACCOUNT_BY_ID");
                    stmt.set_param(0, 1);

                    UserRow user = co_await self->server_->db().Async.execute_prepared<UserRow>(stmt);

                    std::cout << "[Server][Async] id: " << user.id
                              << ", name: " << user.name << "\n";

                } catch (const std::exception& ex) {
                    std::cerr << "[Server] Async DB error: " << ex.what() << "\n";
                }
                co_return;
            });

            Packet resp;
            resp.opcode = Opcode::SMSG_DATABASE_ASYNC_EXAMPLE;
            send_packet(resp);

            break;
        }
        case Opcode::CMSG_DATABASE_SYNC_EXAMPLE: {
            std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode) << "] CMSG_DATABASE_SYNC_EXAMPLE\n";

            // СИНХРОННЫЙ ЗАПРОС
            auto self = shared_from_this();
            post([self]() {
                try {
                    PreparedStatement stmt("LOGIN_SEL_ACCOUNT_BY_ID");
                    stmt.set_param(0, 2);

                    UserRow user = self->server_->db().Sync.execute_prepared<UserRow>(stmt);

                    std::cout << "[Server][Sync] id: " << user.id
                              << ", name: " << user.name << "\n";

                } catch (const std::exception& ex) {
                    std::cerr << "[Server] Sync DB error: " << ex.what() << "\n";
                }
            });

            Packet resp;
            resp.opcode = Opcode::SMSG_DATABASE_SYNC_EXAMPLE;
            send_packet(resp);
            break;
        }
        case Opcode::CMSG_DATABASE_ASYNC_UPDATE: {
            std::cout << "[Server] " << "opcode[" << static_cast<int>(packet.opcode) << "] CMSG_DATABASE_ASYNC_UPDATE\n";

            // Асинхронный без возврата значений
            auto self = shared_from_this();
            spawn([self]() -> boost::asio::awaitable<void> {
                try {
                    PreparedStatement stmt("UPDATE_SOMETHING");
                    stmt.set_param(0, 1);
                    co_await self->server_->db().Async.execute_prepared<NothingRow>(stmt);

                } catch (const std::exception& ex) {
                    std::cerr << "[Server] Async DB error: " << ex.what() << "\n";
                }
                co_return;
            });

            Packet resp;
            resp.opcode = Opcode::SMSG_DATABASE_ASYNC_UPDATE;
            send_packet(resp);
            break;
        }