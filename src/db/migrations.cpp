#include "db/migrations.hpp"

#include <sqlite3.h>

#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include "db/statement.hpp"
#include "db/transaction.hpp"

namespace bucu::db {

namespace {

// Schema version 1 (docs/database-schema.md section 3).
constexpr std::string_view kVersion1 = R"sql(
CREATE TABLE bookmark (
    id          INTEGER PRIMARY KEY,
    url         TEXT NOT NULL UNIQUE,
    title       TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    flags       INTEGER NOT NULL DEFAULT 0,
    created_at  INTEGER NOT NULL,
    updated_at  INTEGER NOT NULL
);
CREATE TABLE tag (
    id   INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);
CREATE TABLE bookmark_tag (
    bookmark_id INTEGER NOT NULL REFERENCES bookmark (id) ON DELETE CASCADE,
    tag_id      INTEGER NOT NULL REFERENCES tag (id) ON DELETE CASCADE,
    PRIMARY KEY (bookmark_id, tag_id)
) WITHOUT ROWID;
CREATE INDEX bookmark_tag_tag_id ON bookmark_tag (tag_id);
)sql";

constexpr std::array<std::string_view, 1> kMigrations{kVersion1};

Error failed(std::string message) { return Error{ErrorCode::kMigrationFailed, std::move(message)}; }

}  // namespace

std::span<const std::string_view> migrations() { return kMigrations; }

std::expected<int, Error> user_version(sqlite3* connection) {
    auto statement = Statement::prepare(connection, "PRAGMA user_version");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }
    if (auto row = statement->step(); !row) {
        return std::unexpected(std::move(row.error()));
    }
    return static_cast<int>(statement->column_int(0));
}

std::expected<void, Error> migrate(sqlite3* connection, std::span<const std::string_view> steps) {
    // No lock while nothing is pending: a reopen must not wait for another writer.
    const auto current = user_version(connection);
    if (!current) {
        return std::unexpected(failed(current.error().message));
    }
    if (static_cast<std::size_t>(*current) >= steps.size()) {
        return {};
    }
    for (;;) {
        auto transaction = Transaction::begin(connection, true);
        if (!transaction) {
            return std::unexpected(failed(std::move(transaction.error().message)));
        }
        const auto version = user_version(connection);
        if (!version) {
            return std::unexpected(failed(version.error().message));
        }
        if (static_cast<std::size_t>(*version) >= steps.size()) {
            return {};  // up to date; the transaction ends unused
        }
        const std::string sql(steps[static_cast<std::size_t>(*version)]);
        char* raw_message = nullptr;
        if (sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, &raw_message) != SQLITE_OK) {
            std::string message = raw_message != nullptr ? raw_message : "migration step failed";
            sqlite3_free(raw_message);
            return std::unexpected(failed(std::move(message)));
        }
        const std::string set_version = "PRAGMA user_version = " + std::to_string(*version + 1);
        if (sqlite3_exec(connection, set_version.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
            return std::unexpected(failed(sqlite3_errmsg(connection)));
        }
        if (auto committed = transaction->commit(); !committed) {
            return std::unexpected(failed(std::move(committed.error().message)));
        }
    }
}

}  // namespace bucu::db
