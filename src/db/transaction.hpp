#pragma once

#include <expected>

#include "bucu/db/error.hpp"

struct sqlite3;

namespace bucu::db {

// A transaction on an open connection: rolls back when destroyed without `commit`. It must not
// outlive the connection.
class Transaction {
public:
    // `immediate` takes the write lock at once (BEGIN IMMEDIATE), waiting up to the busy timeout.
    static std::expected<Transaction, Error> begin(sqlite3* connection, bool immediate = false);

    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&&) = delete;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction();

    // On failure the transaction stays open and is rolled back when destroyed.
    std::expected<void, Error> commit();

private:
    explicit Transaction(sqlite3* connection) : connection_(connection) {}

    sqlite3* connection_;
    bool open_ = true;
};

}  // namespace bucu::db
