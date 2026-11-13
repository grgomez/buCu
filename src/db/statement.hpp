#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "bucu/db/error.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace bucu::db {

// One prepared SQL statement on an open connection. It must not outlive the connection. Parameter
// and column indexes: parameters start at 1, columns at 0 (SQLite's own convention).
class Statement {
public:
    static std::expected<Statement, Error> prepare(sqlite3* connection, std::string_view sql);

    std::expected<void, Error> bind(int index, std::int64_t value);
    std::expected<void, Error> bind(int index, std::string_view value);

    // Runs the statement one step: true when a row is ready, false when it is done.
    std::expected<bool, Error> step();

    // Makes the statement ready to run again; bound values stay until bound again.
    std::expected<void, Error> reset();

    std::int64_t column_int(int index) const;
    std::string column_text(int index) const;

private:
    struct Finalizer {
        void operator()(sqlite3_stmt* statement) const noexcept;
    };

    Statement(sqlite3* connection, sqlite3_stmt* statement)
        : connection_(connection), statement_(statement) {}

    std::expected<void, Error> check(int code) const;

    sqlite3* connection_;
    std::unique_ptr<sqlite3_stmt, Finalizer> statement_;
};

}  // namespace bucu::db
