#include "bucu/db/database.hpp"

#include <sqlite3.h>

#include <string>
#include <system_error>
#include <utility>

#include "db/migrations.hpp"

namespace bucu::db {

namespace {

constexpr int kBusyTimeoutMs = 5000;  // database-schema section 1

std::string message_of(sqlite3* connection, int code) {
    return connection != nullptr ? sqlite3_errmsg(connection) : sqlite3_errstr(code);
}

// Runs a statement that returns no rows.
std::expected<void, Error> exec(sqlite3* connection, const char* sql) {
    char* raw_message = nullptr;
    const int code = sqlite3_exec(connection, sql, nullptr, nullptr, &raw_message);
    if (code != SQLITE_OK) {
        std::string message = raw_message != nullptr ? raw_message : sqlite3_errstr(code);
        sqlite3_free(raw_message);
        return std::unexpected(Error{ErrorCode::kSql, std::move(message)});
    }
    return {};
}

// Asks for WAL. A file system that cannot do WAL keeps the default journal, which is not an
// error (database-schema section 1).
std::expected<void, Error> enable_wal(sqlite3* connection) {
    sqlite3_stmt* statement = nullptr;
    int code = sqlite3_prepare_v2(connection, "PRAGMA journal_mode = WAL", -1, &statement, nullptr);
    if (code == SQLITE_OK) {
        code = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    if (code != SQLITE_ROW && code != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::kSql, message_of(connection, code)});
    }
    return {};
}

}  // namespace

void Database::Closer::operator()(sqlite3* connection) const noexcept { sqlite3_close(connection); }

std::expected<Database, Error> Database::open(const std::filesystem::path& path) {
    std::error_code status;
    const bool present = std::filesystem::exists(path, status);
    if (status) {
        return std::unexpected(Error{ErrorCode::kCannotOpen, status.message()});
    }

    const std::u8string utf8 = path.u8string();
    const std::string name(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    const int flags = SQLITE_OPEN_READWRITE | (present ? 0 : SQLITE_OPEN_CREATE);
    sqlite3* raw = nullptr;
    const int code = sqlite3_open_v2(name.c_str(), &raw, flags, nullptr);
    Database database(raw);  // closes the connection on every return below
    if (code != SQLITE_OK) {
        return std::unexpected(Error{ErrorCode::kCannotOpen, message_of(raw, code)});
    }

    if (auto result = exec(raw, "PRAGMA foreign_keys = ON"); !result) {
        return std::unexpected(std::move(result.error()));
    }
    sqlite3_busy_timeout(raw, kBusyTimeoutMs);
    if (auto result = enable_wal(raw); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = migrate(raw, migrations()); !result) {
        return std::unexpected(std::move(result.error()));
    }
    const auto version = user_version(raw);
    if (!version) {
        return std::unexpected(version.error());
    }
    database.schema_version_ = *version;
    return database;
}

}  // namespace bucu::db
