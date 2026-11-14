#include "bucu/db/bookmarks.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bucu/core/bookmark.hpp"
#include "catch_amalgamated.hpp"
#include "db/handle.hpp"
#include "db/statement.hpp"
#include "support/temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::db::ErrorCode;
using bucu::db::Statement;
using bucu::test::TempDir;

constexpr std::int64_t kNow = 1700000000;

struct Fixture {
    TempDir dir;
    Database database = open_database();

    std::int64_t count(const char* table) {
        auto statement = Statement::prepare(bucu::db::detail::handle(database),
                                            (std::string("SELECT count(*) FROM ") + table).c_str());
        REQUIRE(statement.has_value());
        REQUIRE(statement->step().value());
        return statement->column_int(0);
    }

    void exec(const char* sql) {
        REQUIRE(sqlite3_exec(bucu::db::detail::handle(database), sql, nullptr, nullptr, nullptr) ==
                SQLITE_OK);
    }

    // The tag names of a bookmark, in name order.
    std::vector<std::string> tags_of(std::int64_t id) {
        auto statement =
            Statement::prepare(bucu::db::detail::handle(database),
                               "SELECT tag.name FROM bookmark_tag JOIN tag ON tag.id = tag_id "
                               "WHERE bookmark_id = ? ORDER BY tag.name");
        REQUIRE(statement.has_value());
        REQUIRE(statement->bind(1, id).has_value());
        std::vector<std::string> names;
        while (statement->step().value()) {
            names.push_back(statement->column_text(0));
        }
        return names;
    }

private:
    Database open_database() const {
        auto opened = Database::open(dir.path() / "bookmarks.db");
        REQUIRE(opened.has_value());
        return std::move(*opened);
    }
};

bucu::core::Bookmark make(const char* url, const char* tags = "", const char* title = "",
                          const char* description = "") {
    auto bookmark = bucu::core::make_bookmark(url, title, description, tags);
    REQUIRE(bookmark.has_value());
    return std::move(*bookmark);
}

}  // namespace

TEST_CASE("insert_bookmark stores the bookmark and links its tags", "[db]") {
    Fixture fixture;
    auto bookmark = make("https://example.org/", "web, dev", "Example", "A page");
    bookmark.flags = bucu::core::kFlagImmutableTitle;

    const auto id = bucu::db::insert_bookmark(fixture.database, bookmark, kNow);

    REQUIRE(id.has_value());
    auto row = Statement::prepare(
        bucu::db::detail::handle(fixture.database),
        "SELECT id, url, title, description, flags, created_at, updated_at FROM bookmark");
    REQUIRE(row.has_value());
    REQUIRE(row->step().value());
    CHECK(row->column_int(0) == *id);
    CHECK(row->column_text(1) == "https://example.org/");
    CHECK(row->column_text(2) == "Example");
    CHECK(row->column_text(3) == "A page");
    CHECK(row->column_int(4) == 1);
    CHECK(row->column_int(5) == kNow);
    CHECK(row->column_int(6) == kNow);
    CHECK(fixture.tags_of(*id) == std::vector<std::string>{"dev", "web"});
}

TEST_CASE("insert_bookmark stores a bookmark with no tags", "[db]") {
    Fixture fixture;

    const auto id = bucu::db::insert_bookmark(fixture.database, make("https://example.org/"), kNow);

    REQUIRE(id.has_value());
    CHECK(fixture.count("bookmark") == 1);
    CHECK(fixture.count("tag") == 0);
    CHECK(fixture.count("bookmark_tag") == 0);
}

TEST_CASE("insert_bookmark reuses a tag that already exists", "[db]") {
    Fixture fixture;
    REQUIRE(bucu::db::insert_bookmark(fixture.database, make("https://one.example/", "a, b"), kNow)
                .has_value());

    const auto second =
        bucu::db::insert_bookmark(fixture.database, make("https://two.example/", "b, c"), kNow);

    REQUIRE(second.has_value());
    CHECK(fixture.count("tag") == 3);
    CHECK(fixture.tags_of(*second) == std::vector<std::string>{"b", "c"});
}

TEST_CASE("a stored URL returns kDuplicateUrl with its ID and changes nothing", "[db]") {
    Fixture fixture;
    const auto first =
        bucu::db::insert_bookmark(fixture.database, make("https://example.org/", "a", "One"), kNow);
    REQUIRE(first.has_value());

    const auto again = bucu::db::insert_bookmark(
        fixture.database, make("https://example.org/", "b, c", "Two"), kNow + 5);

    REQUIRE_FALSE(again.has_value());
    CHECK(again.error().code == ErrorCode::kDuplicateUrl);
    REQUIRE(again.error().bookmark_id.has_value());
    CHECK(*again.error().bookmark_id == *first);
    CHECK(fixture.count("bookmark") == 1);
    CHECK(fixture.count("tag") == 1);
    CHECK(fixture.count("bookmark_tag") == 1);
}

TEST_CASE("a failure while linking tags rolls the whole insert back", "[db]") {
    Fixture fixture;
    fixture.exec(
        "CREATE TRIGGER refuse BEFORE INSERT ON bookmark_tag "
        "BEGIN SELECT RAISE(ABORT, 'refused'); END");

    const auto id =
        bucu::db::insert_bookmark(fixture.database, make("https://example.org/", "a"), kNow);

    REQUIRE_FALSE(id.has_value());
    CHECK(id.error().code == ErrorCode::kSql);
    CHECK(fixture.count("bookmark") == 0);
    CHECK(fixture.count("tag") == 0);
}
