#include "db/statement.hpp"

#include <sqlite3.h>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace bucu::db {

namespace {

// SQLITE_TRANSIENT written out: the macro is an old-style cast, which the strict warning set
// rejects (architecture section 8). SQLite documents -1 as "copy the value now".
const sqlite3_destructor_type kCopyValue =
    reinterpret_cast<sqlite3_destructor_type>(static_cast<std::intptr_t>(-1));

}  // namespace

void Statement::Finalizer::operator()(sqlite3_stmt* statement) const noexcept {
    sqlite3_finalize(statement);
}

std::expected<void, Error> Statement::check(int code) const {
    if (code == SQLITE_OK || code == SQLITE_ROW || code == SQLITE_DONE) {
        return {};
    }
    return std::unexpected(Error{ErrorCode::kSql, sqlite3_errmsg(connection_), std::nullopt});
}

std::expected<Statement, Error> Statement::prepare(sqlite3* connection, std::string_view sql) {
    sqlite3_stmt* raw = nullptr;
    const int code =
        sqlite3_prepare_v3(connection, sql.data(), static_cast<int>(sql.size()), 0, &raw, nullptr);
    Statement statement(connection, raw);
    if (auto result = statement.check(code); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return statement;
}

std::expected<void, Error> Statement::bind(int index, std::int64_t value) {
    return check(sqlite3_bind_int64(statement_.get(), index, value));
}

std::expected<void, Error> Statement::bind(int index, std::string_view value) {
    return check(sqlite3_bind_text64(statement_.get(), index, value.data(), value.size(),
                                     kCopyValue, SQLITE_UTF8));
}

std::expected<bool, Error> Statement::step() {
    const int code = sqlite3_step(statement_.get());
    if (auto result = check(code); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return code == SQLITE_ROW;
}

std::expected<void, Error> Statement::reset() {
    // sqlite3_reset repeats the error of the last step; the caller already has it from step().
    sqlite3_reset(statement_.get());
    return {};
}

std::int64_t Statement::column_int(int index) const {
    return sqlite3_column_int64(statement_.get(), index);
}

std::string Statement::column_text(int index) const {
    const unsigned char* text = sqlite3_column_text(statement_.get(), index);
    if (text == nullptr) {
        return {};
    }
    const int size = sqlite3_column_bytes(statement_.get(), index);
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(size));
}

}  // namespace bucu::db
