#include "db/transaction.hpp"

#include <sqlite3.h>

#include <utility>

#include "db/statement.hpp"

namespace bucu::db {

namespace {

std::expected<void, Error> run(sqlite3* connection, const char* sql) {
    auto statement = Statement::prepare(connection, sql);
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }
    if (auto result = statement->step(); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return {};
}

}  // namespace

std::expected<Transaction, Error> Transaction::begin(sqlite3* connection, bool immediate) {
    if (auto result = run(connection, immediate ? "BEGIN IMMEDIATE" : "BEGIN"); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return Transaction(connection);
}

Transaction::Transaction(Transaction&& other) noexcept
    : connection_(other.connection_), open_(std::exchange(other.open_, false)) {}

Transaction::~Transaction() {
    if (open_) {
        sqlite3_exec(connection_, "ROLLBACK", nullptr, nullptr, nullptr);
    }
}

std::expected<void, Error> Transaction::commit() {
    auto result = run(connection_, "COMMIT");
    if (result) {
        open_ = false;
    }
    return result;
}

}  // namespace bucu::db
