#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "bucu/db/database.hpp"
#include "catch_amalgamated.hpp"
#include "db/handle.hpp"
#include "db/migrations.hpp"
#include "db/statement.hpp"
#include "temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::db::ErrorCode;
using bucu::db::Statement;
using bucu::db::test::TempDir;

std::int64_t scalar(Database& database, const char* sql) {
    auto statement = Statement::prepare(bucu::db::detail::handle(database), sql);
    REQUIRE(statement.has_value());
    REQUIRE(statement->step().value());
    return statement->column_int(0);
}

void run(Database& database, const char* sql) {
    auto statement = Statement::prepare(bucu::db::detail::handle(database), sql);
    REQUIRE(statement.has_value());
    REQUIRE(statement->step().has_value());
}

std::vector<std::string> columns_of(Database& database, const std::string& table) {
    auto statement = Statement::prepare(bucu::db::detail::handle(database),
                                        "SELECT name FROM pragma_table_info('" + table + "')");
    REQUIRE(statement.has_value());
    std::vector<std::string> names;
    while (statement->step().value()) {
        names.push_back(statement->column_text(0));
    }
    return names;
}

}  // namespace

TEST_CASE("a new database is at version 1 with the version 1 tables and no rows", "[db]") {
    const TempDir dir;

    auto database = Database::open(dir.path() / "bookmarks.db");

    REQUIRE(database.has_value());
    CHECK(database->schema_version() == 1);
    CHECK(scalar(*database, "PRAGMA user_version") == 1);
    const std::map<std::string, std::vector<std::string>> expected{
        {"bookmark", {"id", "url", "title", "description", "flags", "created_at", "updated_at"}},
        {"tag", {"id", "name"}},
        {"bookmark_tag", {"bookmark_id", "tag_id"}},
    };
    for (const auto& [table, names] : expected) {
        CHECK(columns_of(*database, table) == names);
        CHECK(scalar(*database, ("SELECT count(*) FROM " + table).c_str()) == 0);
    }
}

TEST_CASE("an empty file is upgraded to version 1", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "empty.db";
    std::ofstream(path).close();

    auto database = Database::open(path);

    REQUIRE(database.has_value());
    CHECK(database->schema_version() == 1);
    CHECK(scalar(*database, "PRAGMA user_version") == 1);
    CHECK(scalar(*database, "SELECT count(*) FROM sqlite_master WHERE type = 'table'") == 3);
}

TEST_CASE("reopening a version 1 database keeps every row", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    {
        auto database = Database::open(path);
        REQUIRE(database.has_value());
        run(*database,
            "INSERT INTO bookmark (id, url, title, created_at, updated_at) "
            "VALUES (7, 'https://example.org/', 'Example', 100, 200)");
        run(*database, "INSERT INTO tag (id, name) VALUES (3, 'news')");
        run(*database, "INSERT INTO bookmark_tag (bookmark_id, tag_id) VALUES (7, 3)");
    }

    auto database = Database::open(path);

    REQUIRE(database.has_value());
    CHECK(database->schema_version() == 1);
    CHECK(scalar(*database, "SELECT count(*) FROM bookmark") == 1);
    CHECK(scalar(*database, "SELECT created_at FROM bookmark WHERE id = 7") == 100);
    CHECK(scalar(*database, "SELECT updated_at FROM bookmark WHERE id = 7") == 200);
    CHECK(scalar(*database, "SELECT count(*) FROM tag WHERE id = 3 AND name = 'news'") == 1);
    CHECK(scalar(*database, "SELECT count(*) FROM bookmark_tag") == 1);
}

TEST_CASE("a step that fails part-way rolls back to the version and rows from before", "[db]") {
    const TempDir dir;
    auto database = Database::open(dir.path() / "bookmarks.db");
    REQUIRE(database.has_value());
    run(*database, "INSERT INTO tag (id, name) VALUES (1, 'kept')");
    // The second step creates a table, then fails on a missing one.
    const std::string_view failing =
        "CREATE TABLE extra (a); INSERT INTO no_such_table VALUES (1);";
    std::vector<std::string_view> steps(bucu::db::migrations().begin(),
                                        bucu::db::migrations().end());
    steps.push_back(failing);

    const auto result = bucu::db::migrate(bucu::db::detail::handle(*database), steps);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ErrorCode::kMigrationFailed);
    CHECK(result.error().message.find("no_such_table") != std::string::npos);
    CHECK(scalar(*database, "PRAGMA user_version") == 1);
    CHECK(scalar(*database, "SELECT count(*) FROM sqlite_master WHERE name = 'extra'") == 0);
    CHECK(scalar(*database, "SELECT count(*) FROM tag WHERE name = 'kept'") == 1);
}
