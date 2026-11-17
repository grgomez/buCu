#include "bucu/db/bookmarks.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/id_selection.hpp"
#include "catch_amalgamated.hpp"
#include "db/handle.hpp"
#include "db/statement.hpp"
#include "support/temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::db::ErrorCode;
using bucu::db::Statement;
using bucu::test::TempDir;

constexpr std::int64_t kNowSeconds = 1700000000;
constexpr std::chrono::sys_seconds kNow{std::chrono::seconds{kNowSeconds}};

struct Fixture {
    TempDir dir;
    Database database = open_database();

    std::int64_t count(const char* table) {
        auto statement = Statement::prepare(bucu::db::detail::handle(database),
                                            std::string("SELECT count(*) FROM ") + table);
        REQUIRE(statement.has_value());
        REQUIRE(statement->step().value());
        return statement->column_int(0);
    }

    void exec(const char* sql) {
        REQUIRE(sqlite3_exec(bucu::db::detail::handle(database), sql, nullptr, nullptr, nullptr) ==
                SQLITE_OK);
    }

    // The tag names of a bookmark, in name order.
    std::vector<std::string> tags_of(bucu::core::BookmarkId id) {
        auto statement =
            Statement::prepare(bucu::db::detail::handle(database),
                               "SELECT tag.name FROM bookmark_tag JOIN tag ON tag.id = tag_id "
                               "WHERE bookmark_id = ? ORDER BY tag.name");
        REQUIRE(statement.has_value());
        REQUIRE(statement->bind(1, id.value()).has_value());
        std::vector<std::string> names;
        while (statement->step().value()) {
            names.push_back(statement->column_text(0));
        }
        return names;
    }

private:
    [[nodiscard]] Database open_database() const {
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

// The selection that the ID arguments name.
bucu::core::IdSelection select(const std::vector<std::string>& arguments) {
    auto selection = bucu::core::parse_id_selection(arguments);
    REQUIRE(selection.has_value());
    return std::move(*selection);
}

// Inserts bookmarks 1 to `count`; bookmark n has URL https://n.example/, title "Title n", and tag
// "t<n>" and "all".
void insert_numbered(Fixture& fixture, int count) {
    for (int n = 1; n <= count; ++n) {
        const std::string number = std::to_string(n);
        const auto id = bucu::db::insert_bookmark(
            fixture.database,
            make(("https://" + number + ".example/").c_str(), ("all, t" + number).c_str(),
                 ("Title " + number).c_str()),
            kNow);
        REQUIRE(id.has_value());
        REQUIRE(id->value() == n);
    }
}

// Bookmarks 3, 5, 8, 9, and 11 are stored.
void insert_with_gaps(Fixture& fixture) {
    insert_numbered(fixture, 11);
    fixture.exec("DELETE FROM bookmark WHERE id IN (1, 2, 4, 6, 7, 10)");
}

std::vector<std::int64_t> visited(Fixture& fixture, const std::vector<std::string>& arguments) {
    std::vector<std::int64_t> ids;
    const auto done =
        bucu::db::for_each_bookmark(fixture.database, select(arguments),
                                    [&ids](bucu::core::BookmarkId id, const bucu::core::Bookmark&) {
                                        ids.push_back(id.value());
                                        return true;
                                    });
    REQUIRE(done.has_value());
    return ids;
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
    CHECK(row->column_int(0) == id->value());
    CHECK(row->column_text(1) == "https://example.org/");
    CHECK(row->column_text(2) == "Example");
    CHECK(row->column_text(3) == "A page");
    CHECK(row->column_int(4) == 1);
    CHECK(row->column_int(5) == kNowSeconds);
    CHECK(row->column_int(6) == kNowSeconds);
    CHECK(fixture.tags_of(*id) == std::vector<std::string>{"dev", "web"});
}

TEST_CASE("insert_bookmark stores the flags bit for bit, unknown and high bits included", "[db]") {
    Fixture fixture;
    auto bookmark = make("https://example.org/", "", "", "");
    constexpr std::int64_t kUnknownFlagBits = 6;
    bookmark.flags = bucu::core::FlagSet(INT64_MIN | kUnknownFlagBits);

    REQUIRE(bucu::db::insert_bookmark(fixture.database, bookmark, kNow).has_value());

    auto row = Statement::prepare(bucu::db::detail::handle(fixture.database),
                                  "SELECT flags FROM bookmark");
    REQUIRE(row.has_value());
    REQUIRE(row->step().value());
    CHECK(row->column_int(0) == (INT64_MIN | 6));
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

    const auto again =
        bucu::db::insert_bookmark(fixture.database, make("https://example.org/", "b, c", "Two"),
                                  kNow + std::chrono::seconds{5});

    REQUIRE_FALSE(again.has_value());
    CHECK(again.error().code == ErrorCode::kDuplicateUrl);
    REQUIRE(again.error().bookmark_id.has_value());
    CHECK(again.error().bookmark_id == *first);
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

TEST_CASE("count_bookmarks counts the stored bookmarks", "[db]") {
    Fixture fixture;
    CHECK(bucu::db::count_bookmarks(fixture.database).value() == 0);

    insert_numbered(fixture, 3);

    CHECK(bucu::db::count_bookmarks(fixture.database).value() == 3);
}

TEST_CASE("for_each_bookmark gives every stored bookmark with its tags in ascending ID order",
          "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);
    fixture.exec("UPDATE bookmark SET flags = 5 WHERE id = 8");
    std::vector<std::pair<std::int64_t, bucu::core::Bookmark>> seen;

    const auto done = bucu::db::for_each_bookmark(
        fixture.database, select({}),
        [&seen](bucu::core::BookmarkId id, const bucu::core::Bookmark& bookmark) {
            seen.emplace_back(id.value(), bookmark);
            return true;
        });

    REQUIRE(done.has_value());
    REQUIRE(seen.size() == 5);
    CHECK(seen[0].first == 3);
    CHECK(seen[0].second == bucu::core::Bookmark{.url = "https://3.example/",
                                                 .title = "Title 3",
                                                 .description = "",
                                                 .tags = {"all", "t3"},
                                                 .flags = bucu::core::FlagSet()});
    CHECK(seen[2].first == 8);
    CHECK(seen[2].second.flags == bucu::core::FlagSet(5));
    CHECK(seen[4].first == 11);
}

TEST_CASE("for_each_bookmark gives a bookmark with no tags an empty tag set", "[db]") {
    Fixture fixture;
    REQUIRE(bucu::db::insert_bookmark(fixture.database, make("https://example.org/"), kNow)
                .has_value());
    std::size_t tag_count = 99;

    const auto done = bucu::db::for_each_bookmark(
        fixture.database, select({"1"}),
        [&tag_count](bucu::core::BookmarkId, const bucu::core::Bookmark& bookmark) {
            tag_count = bookmark.tags.size();
            return true;
        });

    REQUIRE(done.has_value());
    CHECK(tag_count == 0);
}

TEST_CASE("for_each_bookmark walks single IDs and ranges with gaps in ascending order", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    CHECK(visited(fixture, {"3,5", "11-8"}) == std::vector<std::int64_t>{3, 5, 8, 9, 11});
    CHECK(visited(fixture, {"6-10"}) == std::vector<std::int64_t>{8, 9});
    CHECK(visited(fixture, {"11", "3"}) == std::vector<std::int64_t>{3, 11});
}

TEST_CASE("for_each_bookmark skips an ID that is not stored", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    CHECK(visited(fixture, {"4"}).empty());
    CHECK(visited(fixture, {"20-30"}).empty());
    CHECK(visited(fixture, {"4,5,99"}) == std::vector<std::int64_t>{5});
}

TEST_CASE("for_each_bookmark gives a bookmark once when items overlap", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    CHECK(visited(fixture, {"3", "3-9", "5", "8-11"}) == std::vector<std::int64_t>{3, 5, 8, 9, 11});
}

TEST_CASE("for_each_bookmark covers a huge range by the stored bookmarks only", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    CHECK(visited(fixture, {"1-9999999999"}) == std::vector<std::int64_t>{3, 5, 8, 9, 11});
    CHECK(visited(fixture, {"1-9223372036854775807"}) == std::vector<std::int64_t>{3, 5, 8, 9, 11});
}

TEST_CASE("for_each_bookmark stops when the visitor returns false and reports success", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);
    std::vector<std::int64_t> ids;

    const auto done =
        bucu::db::for_each_bookmark(fixture.database, select({}),
                                    [&ids](bucu::core::BookmarkId id, const bucu::core::Bookmark&) {
                                        ids.push_back(id.value());
                                        return ids.size() < 2;
                                    });

    REQUIRE(done.has_value());
    CHECK(ids == std::vector<std::int64_t>{3, 5});
}

TEST_CASE("for_each_bookmark on an empty database visits nothing", "[db]") {
    Fixture fixture;

    CHECK(visited(fixture, {}).empty());
    CHECK(visited(fixture, {"1-5"}).empty());
}
