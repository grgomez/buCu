#pragma once

#include <expected>
#include <string>
#include <vector>

#include "bucu/db/database.hpp"
#include "bucu/db/error.hpp"

namespace bucu::db {

// Runs SQLite's full integrity check and returns the problems it reports, as SQLite words them
// (a problem may hold line breaks). An empty list means the database is intact. SQLite reports at
// most 100 problems. When the check itself fails part way, as it does on a damaged file, the
// failure message is the last line of the report rather than an error. An error means the check
// could not start.
std::expected<std::vector<std::string>, Error> integrity_check(Database& database);

}  // namespace bucu::db
