#include "bucu/db/database.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <string>

#include "catch_amalgamated.hpp"
#include "db/handle.hpp"
#include "temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::db::ErrorCode;
using bucu::db::test::TempDir;

// Reads the first column of the first row of `sql` on the connection `open` configured.
std::string query_text(Database& database, const char* sql) {
    sqlite3* const handle = bucu::db::detail::handle(database);
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
    const unsigned char* text = sqlite3_column_text(statement, 0);
    std::string value(text == nullptr ? "" : reinterpret_cast<const char*>(text));
    sqlite3_finalize(statement);
    return value;
}

}  // namespace

TEST_CASE("open creates a missing file", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";

    auto database = Database::open(path);

    REQUIRE(database.has_value());
    CHECK(std::filesystem::exists(path));
}

TEST_CASE(
    "open enforces foreign keys, WAL, and a 5-second busy timeout (database-schema section 1)",
    "[db]") {
    const TempDir dir;
    auto database = Database::open(dir.path() / "bookmarks.db");
    REQUIRE(database.has_value());

    CHECK(query_text(*database, "PRAGMA foreign_keys") == "1");
    CHECK(query_text(*database, "PRAGMA journal_mode") == "wal");
    CHECK(query_text(*database, "PRAGMA busy_timeout") == "5000");
}

TEST_CASE("open creates a database at a non-ASCII UTF-8 path", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / std::filesystem::path(u8"données-書籤.db");

    auto database = Database::open(path);

    REQUIRE(database.has_value());
    CHECK(std::filesystem::exists(path));
}

TEST_CASE("open returns an error naming the cause instead of ending the program", "[db]") {
    const TempDir dir;

    auto database = Database::open(dir.path() / "missing-directory" / "bookmarks.db");

    REQUIRE_FALSE(database.has_value());
    CHECK(database.error().code == ErrorCode::kCannotOpen);
    CHECK_FALSE(database.error().message.empty());
}
