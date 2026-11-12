#pragma once

#include <expected>
#include <span>
#include <string_view>

#include "bucu/db/error.hpp"

struct sqlite3;

namespace bucu::db {

// The migration steps in order: step n moves the schema from version n to n + 1. The SQL of each
// step is the exact shape of that version (architecture section 5).
std::span<const std::string_view> migrations();

// Reads `PRAGMA user_version`.
std::expected<int, Error> user_version(sqlite3* connection);

// Applies every step of `steps` the file has not had yet, each in its own `BEGIN IMMEDIATE`
// transaction that re-reads the version first (another process may have migrated the file while
// this one waited for the lock). A failed step, a lock wait that runs out included, rolls back
// and returns kMigrationFailed.
std::expected<void, Error> migrate(sqlite3* connection, std::span<const std::string_view> steps);

}  // namespace bucu::db
