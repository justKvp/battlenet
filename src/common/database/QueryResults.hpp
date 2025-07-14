#pragma once

#include <pqxx/pqxx>
#include <string>
#include <chrono>

#include "TimeUtils.hpp"

// === Структуры ===

struct AccountSaltAndVerifierRow {
    int64_t id;
    std::string name;
    std::string salt;
    std::string verifier;
    std::string email;
    std::chrono::system_clock::time_point created_at;
};

struct NothingRow {};

// === Шаблон PgRowMapper ===

template<typename T>
struct PgRowMapper;

template<>
struct PgRowMapper<AccountSaltAndVerifierRow> {
    static AccountSaltAndVerifierRow map(const pqxx::row &r) {
        AccountSaltAndVerifierRow row;
        row.id = r["id"].as<int64_t>();
        row.name = r["username"].as<std::string>();
        row.salt = r["salt"].as<std::string>();
        row.verifier = r["verifier"].as<std::string>();
        row.email = r["email"].as<std::string>();
        row.created_at = TimeUtils::parse_pg_timestamp(r["created_at"].as<std::string>());
        return row;
    }
};

template<>
struct PgRowMapper<NothingRow> {
    static NothingRow map(const pqxx::row &) {
        return NothingRow{};
    }
};

template<>
struct PgRowMapper<int64_t> {
    static int64_t map(const pqxx::row &r) {
        return r[0].as<int64_t>();
    }
};

//template<>
//struct PgRowMapper<long> {
//    static long map(const pqxx::row &r) {
//        return r[0].as<long>();
//    }
//};
