#pragma once

#include "PreparedStatement.hpp"
#include "QueryResults.hpp"

#include <pqxx/pqxx>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <future>
#include <type_traits>

using boost::asio::awaitable;

class Database {
public:
    Database(boost::asio::thread_pool &pool, const std::string &conninfo)
            : pool_(pool), connection_(conninfo) {
        pqxx::work txn(connection_);
        connection_.prepare("LOGIN_SEL_ACCOUNT_BY_ID",
                            "SELECT id, name FROM users WHERE id = $1");
        connection_.prepare("UPDATE_SOMETHING",
                            "UPDATE users SET name = $1 WHERE id = $2");
        txn.commit();
    }

    boost::asio::thread_pool& thread_pool() { return pool_; }

    struct AsyncAPI {
        Database &db;

        template<typename Func, typename ResultType = typename std::invoke_result_t<Func>>
        awaitable<ResultType> async_db_call(Func &&func) {
            std::packaged_task<ResultType()> task(std::forward<Func>(func));
            auto fut = task.get_future();
            boost::asio::post(db.pool_, std::move(task));
            co_return fut.get();
        }

        template<typename Struct>
        awaitable<Struct> execute_prepared(PreparedStatement stmt) {
            co_return co_await async_db_call([this, stmt = std::move(stmt)] {
                return db.execute_prepared_sync<Struct>(stmt);
            });
        }
    };

    struct SyncAPI {
        Database &db;

        template<typename Struct>
        Struct execute_prepared(PreparedStatement stmt) {
            return db.execute_prepared_sync<Struct>(stmt);
        }
    };

    AsyncAPI Async{*this};
    SyncAPI Sync{*this};

private:
    template<typename Struct>
    Struct execute_prepared_sync(PreparedStatement stmt) {
        pqxx::work txn(connection_);

        std::vector<const char*> params;
        for (const auto &param : stmt.params()) {
            if (param.has_value()) {
                params.push_back(param.value().c_str());
            } else {
                params.push_back(nullptr);
            }
        }

        pqxx::result result = txn.exec_prepared(stmt.name(), params.data(), params.data() + params.size());
        txn.commit();

        if constexpr (std::is_same_v<Struct, NothingRow>) {
            return {};
        }

        if (result.empty()) {
            throw std::runtime_error("No rows found");
        }

        return PgRowMapper<Struct>::map(result[0]);
    }


    boost::asio::thread_pool &pool_;
    pqxx::connection connection_;
};
