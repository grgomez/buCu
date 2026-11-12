#include "bucu/db/database.hpp"

#include <sqlite3.h>

#include <string>
#include <system_error>
#include <utility>

#include "db/handle.hpp"
#include "db/migrations.hpp"
#include "db/statement.hpp"

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

// Version 0 means an empty file; tables in a version 0 file belong to another program. Reads
// sqlite_master, which every SQLite version has (sqlite_schema needs 3.33.0).
std::expected<bool, Error> has_tables(sqlite3* connection) {
    auto statement = Statement::prepare(connection, "SELECT count(*) FROM sqlite_master");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }
    if (auto row = statement->step(); !row) {
        return std::unexpected(std::move(row.error()));
    }
    return statement->column_int(0) > 0;
}

}  // namespace

void Database::Closer::operator()(sqlite3* connection) const noexcept { sqlite3_close(connection); }

namespace detail {

void ConnectionCloser::operator()(sqlite3* connection) const noexcept { sqlite3_close(connection); }

using Connection = std::unique_ptr<sqlite3, ConnectionCloser>;

// A failed first open keeps the file it created: an empty version 0 database that the next open
// upgrades. BuCu never deletes a database file (docs/database-schema.md section 2).
std::expected<Connection, Error> open_connection(const std::filesystem::path& path,
                                                 std::span<const std::string_view> steps) {
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
    std::unique_ptr<sqlite3, ConnectionCloser> connection(raw);  // closed on every return below
    if (code != SQLITE_OK) {
        return std::unexpected(Error{ErrorCode::kCannotOpen, message_of(raw, code)});
    }
    // SQLite quietly opens a write-protected file read-only; BuCu cannot use it (a later write
    // would fail), so refuse it here.
    if (sqlite3_db_readonly(raw, "main") == 1) {
        return std::unexpected(
            Error{ErrorCode::kCannotOpen, "the file cannot be written (it is read-only)"});
    }

    if (auto result = exec(raw, "PRAGMA foreign_keys = ON"); !result) {
        return std::unexpected(std::move(result.error()));
    }
    sqlite3_busy_timeout(raw, kBusyTimeoutMs);
    // Every check that can refuse comes before the first write, so a refusal leaves the file's
    // bytes unchanged (docs/database-schema.md section 2). Setting WAL writes the header.
    const auto found = user_version(raw);
    if (!found) {
        if (sqlite3_errcode(raw) == SQLITE_NOTADB) {
            return std::unexpected(
                Error{ErrorCode::kNotADatabase, "the file is not a SQLite database"});
        }
        return std::unexpected(found.error());
    }
    if (*found < 0) {
        return std::unexpected(
            Error{ErrorCode::kNotBucuDatabase, "the file has an invalid schema version (" +
                                                   std::to_string(*found) +
                                                   "), so it is not a BuCu database"});
    }
    if (*found > kSchemaVersion) {
        return std::unexpected(Error{ErrorCode::kNewerSchema,
                                     "the database has schema version " + std::to_string(*found) +
                                         ", newer than version " + std::to_string(kSchemaVersion) +
                                         ", the newest this BuCu supports"});
    }
    if (*found == 0) {
        const auto tables = has_tables(raw);
        if (!tables) {
            return std::unexpected(tables.error());
        }
        if (*tables) {
            return std::unexpected(
                Error{ErrorCode::kNotBucuDatabase,
                      "the file has tables but no schema version, so it is not a BuCu database"});
        }
    }
    if (auto result = enable_wal(raw); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = migrate(raw, steps); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return connection;
}

}  // namespace detail

std::expected<Database, Error> Database::open(const std::filesystem::path& path) {
    auto connection = detail::open_connection(path, migrations());
    if (!connection) {
        return std::unexpected(std::move(connection.error()));
    }
    Database database(connection->release());
    const auto version = user_version(detail::handle(database));
    if (!version) {
        return std::unexpected(version.error());
    }
    database.schema_version_ = *version;
    return database;
}

}  // namespace bucu::db
