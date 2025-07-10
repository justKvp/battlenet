#pragma once

#include <pqxx/pqxx>
#include <string>

// ======================
// Результаты
// ======================
struct UserRow {
    int32_t id;
    std::string name;
};

struct NothingRow {};

// ======================
// Универсальный PgRowMapper
// ======================
template<typename T>
struct PgRowMapper;  // первичная не реализована

// 🎯 Специализация для UserRow
template<>
struct PgRowMapper<UserRow> {
    static UserRow map(const pqxx::row &row) {
        return UserRow{
                .id = row[0].as<int32_t>(),
                .name = row[1].as<std::string>()
        };
    }
};

// 🎯 Специализация для NothingRow
template<>
struct PgRowMapper<NothingRow> {
    static NothingRow map(const pqxx::row &) {
        return {};
    }
};
