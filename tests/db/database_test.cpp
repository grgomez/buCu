#include "bucu/db/database.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <sstream>
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

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

// Makes a file with a plain SQLite connection, without BuCu's code, in the default journal mode.
void make_file(const std::filesystem::path& path, const char* sql) {
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open(bucu::db::test::utf8_path(path).c_str(), &connection) == SQLITE_OK);
    REQUIRE(sqlite3_exec(connection, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(connection) == SQLITE_OK);
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

TEST_CASE("open refuses a newer schema, names both versions, and leaves the file alone", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "newer.db";
    make_file(path, "CREATE TABLE future (a); PRAGMA user_version = 99");
    const std::string before = read_bytes(path);

    auto database = Database::open(path);

    REQUIRE_FALSE(database.has_value());
    CHECK(database.error().code == ErrorCode::kNewerSchema);
    CHECK(database.error().message.find("99") != std::string::npos);
    CHECK(database.error().message.find(std::to_string(bucu::db::kSchemaVersion)) !=
          std::string::npos);
    CHECK(read_bytes(path) == before);
}

TEST_CASE("open refuses a version 0 file that has tables and leaves it alone", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "other.db";
    make_file(path, "CREATE TABLE bookmarks (id INTEGER PRIMARY KEY, url TEXT)");
    const std::string before = read_bytes(path);

    auto database = Database::open(path);

    REQUIRE_FALSE(database.has_value());
    CHECK(database.error().code == ErrorCode::kNotBucuDatabase);
    CHECK(database.error().message.find("not a BuCu database") != std::string::npos);
    CHECK(read_bytes(path) == before);
}

TEST_CASE("open refuses a file that is not a SQLite database and leaves it alone", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "notes.txt";
    {
        std::ofstream file(path, std::ios::binary);
        file << std::string(300, 'x') << "\nthis is plain text, not a database\n";
    }
    const std::string before = read_bytes(path);

    auto database = Database::open(path);

    REQUIRE_FALSE(database.has_value());
    CHECK(database.error().code == ErrorCode::kNotADatabase);
    CHECK(database.error().message.find("not a SQLite database") != std::string::npos);
    CHECK(read_bytes(path) == before);
}

TEST_CASE("open refuses a write-protected version 1 database and leaves it alone", "[db]") {
    namespace fs = std::filesystem;
    const TempDir dir;
    const auto path = dir.path() / "protected.db";
    {
        auto created = Database::open(path);
        REQUIRE(created.has_value());
    }
    const std::string before = read_bytes(path);
    // Restores write access at the end so the directory can be removed on every platform.
    struct Restore {
        const fs::path& file;
        ~Restore() {
            std::error_code ignored;
            fs::permissions(file, fs::perms::owner_write, fs::perm_options::add, ignored);
        }
    } restore{path};
    fs::permissions(path, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write,
                    fs::perm_options::remove);
    if (std::ofstream(path, std::ios::binary | std::ios::app).is_open()) {
        SKIP("the file is still writable (running as the superuser), so it cannot be protected");
    }

    auto database = Database::open(path);

    REQUIRE_FALSE(database.has_value());
    CHECK(database.error().code == ErrorCode::kCannotOpen);
    CHECK_FALSE(database.error().message.empty());
    CHECK(read_bytes(path) == before);
}
