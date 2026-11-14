#pragma once

#include <expected>
#include <filesystem>
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

// Writes a consistent copy of the database to `destination`, a new file, with SQLite's online
// backup. The copy holds the data committed when the backup ran, even while another process
// writes to the database. An existing `destination` is refused with `kDestinationExists` and is
// not touched; a missing parent directory is `kCannotOpen`, and no directory is created. When
// the copy fails, the new file and its -journal, -wal, and -shm files are removed.
std::expected<void, Error> backup(Database& database, const std::filesystem::path& destination);

}  // namespace bucu::db
