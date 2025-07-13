// ✅ Итоговая версия Database с пулом соединений, реконнектом и корректным закрытием

#pragma once

#include "PreparedStatement.hpp"
#include "QueryResults.hpp"
#include "Logger.hpp"

#include <pqxx/pqxx>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#include <future>
#include <optional>
#include <queue>
#include <mutex>
#include <memory>
#include <iostream>

using boost::asio::awaitable;

class Database {
public:
    Database(boost::asio::thread_pool &pool, const std::string &conninfo, size_t pool_size = 4)
            : pool_(pool), conninfo_(conninfo)
    {
        auto log = Logger::get();
        for (size_t i = 0; i < pool_size; ++i) {
            auto conn = std::make_unique<pqxx::connection>(conninfo_);
            prepare_all(*conn);
            log->info("[Database] Connection {} established", i + 1);
            connections_.push(std::move(conn));
        }
    }

    ~Database() {
        shutdown();
    }

    void shutdown() {
        auto log = Logger::get();
        std::lock_guard<std::mutex> lock(mutex_);
        while (!connections_.empty()) {
            auto &c = connections_.front();
            if (c && c->is_open()) {
                c->disconnect();
                log->info("[Database] Connection closed.");
            }
            connections_.pop();
        }
    }

    boost::asio::thread_pool &thread_pool() { return pool_; }

    struct AsyncAPI {
        Database &db;

        template <typename Func, typename ResultType = typename std::invoke_result_t<Func>>
        awaitable<ResultType> async_db_call(Func &&func) {
            std::promise<ResultType> promise;
            auto fut = promise.get_future();

            boost::asio::post(db.pool_,
                              [func = std::forward<Func>(func), promise = std::move(promise)]() mutable {
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
        auto conn = acquire_connection();
        if (!conn) throw std::runtime_error("No DB connection available");

        try {
            pqxx::work txn(*conn);
            auto invoc = txn.prepared(stmt.name());
            for (const auto &param : stmt.params()) {
                if (param.has_value()) {
                    invoc(param.value());
                } else {
                    invoc(static_cast<const char *>(nullptr));
                }
            }

            auto result = invoc.exec();
            txn.commit();

            release_connection(std::move(conn));

            if constexpr (std::is_same_v<Struct, NothingRow>) {
                return Struct{};
            }

            if (result.empty()) {
                return std::nullopt;
            }

            return PgRowMapper<Struct>::map(result[0]);
        }
        catch (const pqxx::broken_connection&) {
            std::cerr << "[Database] Connection broken. Attempting to reconnect.\n";
            conn = reconnect_connection();
            if (!conn) throw std::runtime_error("Reconnection failed");

            release_connection(std::move(conn));
            throw;
        }
    }

    std::unique_ptr<pqxx::connection> acquire_connection() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connections_.empty()) return nullptr;

        auto conn = std::move(connections_.front());
        connections_.pop();

        if (!conn->is_open()) {
            auto log = Logger::get();
            log->warn("[Database] Connection was closed. Reconnecting.");
            conn = reconnect_connection();
        }

        return conn;
    }

    void release_connection(std::unique_ptr<pqxx::connection> conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push(std::move(conn));
    }

    std::unique_ptr<pqxx::connection> reconnect_connection() {
        auto conn = std::make_unique<pqxx::connection>(conninfo_);
        prepare_all(*conn);
        auto log = Logger::get();
        log->info("[Database] Connection re-established.");
        return conn;
    }

    void prepare_all(pqxx::connection &conn) {
        pqxx::work txn(conn);
        conn.prepare("LOGIN_SEL_ACCOUNT_BY_ID",
                     "SELECT id, name FROM users WHERE id = $1");
        conn.prepare("UPDATE_SOMETHING",
                     "UPDATE users SET name = $1 WHERE id = $2");
        txn.commit();
    }

    boost::asio::thread_pool &pool_;
    std::string conninfo_;
    std::queue<std::unique_ptr<pqxx::connection>> connections_;
    std::mutex mutex_;
};
