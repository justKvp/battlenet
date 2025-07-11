#pragma once

#include "PreparedStatement.hpp"
#include "QueryResults.hpp"

#include <pqxx/pqxx>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <future>
#include <optional>
#include <type_traits>

using boost::asio::awaitable;

class Database {
public:
    Database(boost::asio::thread_pool &pool, const std::string &conninfo)
            : pool_(pool), connection_(conninfo) {
        // 🔒 Регистрируем подготовленные запросы один раз
        pqxx::work txn(connection_);
        connection_.prepare("LOGIN_SEL_ACCOUNT_BY_ID",
                            "SELECT id, name FROM users WHERE id = $1");
        connection_.prepare("UPDATE_SOMETHING",
                            "UPDATE users SET name = $1 WHERE id = $2");
        txn.commit();
    }

    boost::asio::thread_pool &thread_pool() { return pool_; }

    // ============================
    // === Асинхронный интерфейс ===
    // ============================
    struct AsyncAPI {
        Database &db;

        template <typename Func, typename ResultType = typename std::invoke_result_t<Func>>
        awaitable<ResultType> async_db_call(Func &&func) {
            std::promise<ResultType> promise;
            auto fut = promise.get_future();

            boost::asio::post(db.pool_, [func = std::forward<Func>(func), promise = std::move(promise)]() mutable {
                try {
                    promise.set_value(func());
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            });

            co_return fut.get();
        }

        template <typename Struct>
        awaitable<std::optional<Struct>> execute(PreparedStatement stmt) {
            co_return co_await async_db_call([this, stmt = std::move(stmt)] {
                return db.execute_sync<Struct>(stmt);
            });
        }
    };

    // ============================
    // === Синхронный интерфейс ===
    // ============================
    struct SyncAPI {
        Database &db;

        template <typename Struct>
        std::optional<Struct> execute(PreparedStatement stmt) {
            return db.execute_sync<Struct>(stmt);
        }
    };

    AsyncAPI Async{*this};
    SyncAPI Sync{*this};

private:
    template <typename Struct>
    std::optional<Struct> execute_sync(const PreparedStatement &stmt) {
        pqxx::work txn(connection_);

        // 🔒 Используем prepared invocation
        auto invoc = txn.prepared(stmt.name());
        for (const auto &param : stmt.params()) {
            if (param.has_value()) {
                invoc(param.value());
            } else {
                invoc(static_cast<const char *>(nullptr));
            }
        }

        pqxx::result result = invoc.exec();
        txn.commit();

        if constexpr (std::is_same_v<Struct, NothingRow>) {
            return Struct{};
        }

        if (result.empty()) {
            return std::nullopt;
        }

        return PgRowMapper<Struct>::map(result[0]);
    }

    boost::asio::thread_pool &pool_;
    pqxx::connection connection_;
};
