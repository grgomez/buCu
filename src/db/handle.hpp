#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

#include "bucu/db/database.hpp"
#include "bucu/db/error.hpp"

struct sqlite3;

namespace bucu::db::detail {

// The connection that Database::open configured. For code and tests inside the db module only:
// no other module runs SQL (architecture section 4).
inline sqlite3* handle(Database& database) { return database.connection_.get(); }

struct ConnectionCloser {
    void operator()(sqlite3* connection) const noexcept;
};

// Opens, checks, and migrates the database at `path` with `steps`: everything `Database::open`
// does, with the step list as a parameter so a test can pass a failing step.
std::expected<std::unique_ptr<sqlite3, ConnectionCloser>, Error> open_connection(
    const std::filesystem::path& path, std::span<const std::string_view> steps);

}  // namespace bucu::db::detail
