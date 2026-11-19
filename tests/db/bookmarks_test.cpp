#include "bucu/db/bookmarks.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/changes.hpp"
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
    auto bookmark = bucu::core::make_bookmark({.url = bucu::core::RequiredText{url},
                                               .title = bucu::core::RequiredText{title},
                                               .description = bucu::core::RequiredText{description},
                                               .tags = tags});
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

// Bookmarks 1 to kGapCount are inserted, then 1, 2, 4, 6, 7, and 10 deleted.
constexpr int kGapCount = 11;

// Bookmarks 3, 5, 8, 9, and 11 are stored.
void insert_with_gaps(Fixture& fixture) {
    insert_numbered(fixture, kGapCount);
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

using bucu::core::ChangeRequest;
using bucu::core::TagChange;
using bucu::core::TagOperation;

constexpr std::chrono::sys_seconds kLater = kNow + std::chrono::seconds{60};

bucu::core::BookmarkChanges changes(const ChangeRequest& request) {
    auto checked = bucu::core::make_changes(request);
    REQUIRE(checked.has_value());
    return std::move(*checked);
}

ChangeRequest set_title(std::string title) { return ChangeRequest{.title = std::move(title)}; }

ChangeRequest tag_change(TagOperation operation, std::string text) {
    return ChangeRequest{.tags = TagChange{.operation = operation, .text = std::move(text)}};
}

// The first column of each row of `sql`, joined with ";". Build a composite with SQL's `||`.
std::string cells(Fixture& fixture, const char* sql) {
    auto statement = Statement::prepare(bucu::db::detail::handle(fixture.database), sql);
    REQUIRE(statement.has_value());
    std::string out;
    while (statement->step().value()) {
        out += out.empty() ? "" : ";";
        out += statement->column_text(0);
    }
    return out;
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
    std::optional<std::size_t> tag_count;

    const auto done = bucu::db::for_each_bookmark(
        fixture.database, select({"1"}),
        [&tag_count](bucu::core::BookmarkId, const bucu::core::Bookmark& bookmark) {
            tag_count = bookmark.tags.size();
            return true;
        });

    REQUIRE(done.has_value());
    CHECK(tag_count == std::optional<std::size_t>(0));
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

namespace {

constexpr const char* kAllRows =
    "SELECT id || '|' || title || '|' || flags || '|' || created_at || '|' || updated_at "
    "FROM bookmark ORDER BY id";
constexpr const char* kAllLinks =
    "SELECT bookmark_id || '|' || name FROM bookmark_tag JOIN tag ON tag.id = tag_id "
    "ORDER BY bookmark_id, name";
constexpr const char* kAllTags = "SELECT name FROM tag ORDER BY name";

}  // namespace

TEST_CASE("update_bookmarks changes the selected bookmarks only", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    const auto done = bucu::db::update_bookmarks(fixture.database, select({"3,5", "9-11"}),
                                                 changes(set_title("New")), kLater);

    REQUIRE(done.has_value());
    CHECK(cells(fixture, kAllRows) ==
          "3|New|0|1700000000|1700000060;5|New|0|1700000000|1700000060;"
          "8|Title 8|0|1700000000|1700000000;9|New|0|1700000000|1700000060;"
          "11|New|0|1700000000|1700000060");
}

TEST_CASE("update_bookmarks with every bookmark selected changes them all", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 3);

    const auto done = bucu::db::update_bookmarks(
        fixture.database, select({}), changes(tag_change(TagOperation::kAdd, "x")), kLater);

    REQUIRE(done.has_value());
    CHECK(fixture.tags_of(bucu::core::BookmarkId(1)) == std::vector<std::string>{"all", "t1", "x"});
    CHECK(fixture.tags_of(bucu::core::BookmarkId(3)) == std::vector<std::string>{"all", "t3", "x"});
}

TEST_CASE("update_bookmarks writes the new URL, description, and flags", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);

    const auto done =
        bucu::db::update_bookmarks(fixture.database, select({"2"}),
                                   changes(ChangeRequest{.url = "HTTPS://New.example:443/x",
                                                         .description = "D",
                                                         .flag = bucu::core::FlagChange::kSet}),
                                   kLater);

    REQUIRE(done.has_value());
    CHECK(cells(fixture,
                "SELECT url || '|' || description || '|' || flags FROM bookmark WHERE id = 2") ==
          "https://new.example/x|D|1");
}

TEST_CASE("update_bookmarks gives a changed bookmark updated_at = now and keeps created_at",
          "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"2"}), changes(set_title("New")),
                                       kLater)
                .has_value());

    CHECK(cells(fixture, "SELECT created_at || '|' || updated_at FROM bookmark WHERE id = 2") ==
          "1700000000|1700000060");
}

TEST_CASE("update_bookmarks leaves a bookmark whose values do not change as it was", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);

    // Bookmark 1 already has this title and tag, so only bookmark 2 changes.
    const auto done = bucu::db::update_bookmarks(
        fixture.database, select({"1-2"}),
        changes(ChangeRequest{.title = "Title 1",
                              .tags = TagChange{.operation = TagOperation::kAdd, .text = "all"}}),
        kLater);

    REQUIRE(done.has_value());
    CHECK(cells(fixture, "SELECT updated_at FROM bookmark ORDER BY id") == "1700000000;1700000060");
}

TEST_CASE("update_bookmarks changes updated_at when only the tags change", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 1);

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1"}),
                                       changes(tag_change(TagOperation::kAdd, "x")), kLater)
                .has_value());

    CHECK(cells(fixture, "SELECT updated_at FROM bookmark") == "1700000060");
}

TEST_CASE("update_bookmarks adds, removes, and replaces tags", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 1);

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1"}),
                                       changes(tag_change(TagOperation::kAdd, "x, y")), kLater)
                .has_value());
    CHECK(fixture.tags_of(bucu::core::BookmarkId(1)) ==
          std::vector<std::string>{"all", "t1", "x", "y"});

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1"}),
                                       changes(tag_change(TagOperation::kRemove, "t1, zz")), kLater)
                .has_value());
    CHECK(fixture.tags_of(bucu::core::BookmarkId(1)) == std::vector<std::string>{"all", "x", "y"});

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1"}),
                                       changes(tag_change(TagOperation::kReplace, "q")), kLater)
                .has_value());
    CHECK(fixture.tags_of(bucu::core::BookmarkId(1)) == std::vector<std::string>{"q"});

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1"}),
                                       changes(tag_change(TagOperation::kReplace, "")), kLater)
                .has_value());
    CHECK(fixture.tags_of(bucu::core::BookmarkId(1)).empty());
}

TEST_CASE("update_bookmarks deletes tags that no bookmark links any more", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1"}),
                                       changes(tag_change(TagOperation::kReplace, "")), kLater)
                .has_value());

    // t1 is gone; "all" is still linked by bookmark 2.
    CHECK(cells(fixture, kAllTags) == "all;t2");
}

TEST_CASE("update_bookmarks keeps a tag another bookmark still links", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1"}),
                                       changes(tag_change(TagOperation::kRemove, "all")), kLater)
                .has_value());

    CHECK(cells(fixture, kAllTags) == "all;t1;t2");
}

TEST_CASE(
    "update_bookmarks reports the first missing single ID by its low end, and changes nothing",
    "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);
    const std::string rows = cells(fixture, kAllRows);

    const auto done = bucu::db::update_bookmarks(fixture.database, select({"9,12", "4", "3"}),
                                                 changes(set_title("New")), kLater);

    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == ErrorCode::kNotFound);
    CHECK(done.error().bookmark_id == bucu::core::BookmarkId(4));
    CHECK_FALSE(done.error().range.has_value());
    CHECK(cells(fixture, kAllRows) == rows);
}

TEST_CASE("update_bookmarks reports a range that holds no stored ID with the range", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    const auto done = bucu::db::update_bookmarks(fixture.database, select({"3", "30-20"}),
                                                 changes(set_title("New")), kLater);

    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == ErrorCode::kNotFound);
    CHECK_FALSE(done.error().bookmark_id.has_value());
    CHECK(done.error().range ==
          std::optional<bucu::core::IdRange>(bucu::core::IdRange{
              .first = bucu::core::BookmarkId(20), .last = bucu::core::BookmarkId(30)}));
}

TEST_CASE("update_bookmarks accepts a range with gaps when it holds a stored ID", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    const auto empty = bucu::db::update_bookmarks(fixture.database, select({"6-7"}),
                                                  changes(set_title("New")), kLater);
    const auto holds_one = bucu::db::update_bookmarks(fixture.database, select({"5-7"}),
                                                      changes(set_title("New")), kLater);

    REQUIRE_FALSE(empty.has_value());
    CHECK(empty.error().range.has_value());
    CHECK(holds_one.has_value());
}

TEST_CASE("update_bookmarks on an empty database is not found, with neither ID nor range", "[db]") {
    Fixture fixture;

    const auto done =
        bucu::db::update_bookmarks(fixture.database, select({}), changes(set_title("New")), kLater);

    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == ErrorCode::kNotFound);
    CHECK_FALSE(done.error().bookmark_id.has_value());
    CHECK_FALSE(done.error().range.has_value());
}

TEST_CASE("update_bookmarks reports not found before a duplicate URL", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);

    const auto done =
        bucu::db::update_bookmarks(fixture.database, select({"7"}),
                                   changes(ChangeRequest{.url = "https://2.example/"}), kLater);

    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == ErrorCode::kNotFound);
}

TEST_CASE("update_bookmarks refuses a URL another bookmark holds, and changes nothing", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);
    const std::string rows = cells(fixture, kAllRows);

    const auto done = bucu::db::update_bookmarks(
        fixture.database, select({"1"}),
        changes(ChangeRequest{.url = "https://2.example/", .title = "New"}), kLater);

    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == ErrorCode::kDuplicateUrl);
    CHECK(done.error().bookmark_id == bucu::core::BookmarkId(2));
    CHECK(cells(fixture, kAllRows) == rows);
}

TEST_CASE("update_bookmarks takes a bookmark's own URL as no conflict", "[db]") {
    Fixture fixture;
    insert_numbered(fixture, 2);

    const auto done = bucu::db::update_bookmarks(
        fixture.database, select({"1"}),
        changes(ChangeRequest{.url = "HTTPS://1.example:443/", .title = "New"}), kLater);

    REQUIRE(done.has_value());
    CHECK(cells(fixture, "SELECT title FROM bookmark WHERE id = 1") == "New");
}

TEST_CASE("a failure part way leaves every row and tag as it was", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);
    const std::string rows = cells(fixture, kAllRows);
    const std::string links = cells(fixture, kAllLinks);
    const std::string tags = cells(fixture, kAllTags);
    fixture.exec(
        "CREATE TRIGGER refuse BEFORE UPDATE ON bookmark WHEN NEW.id = 9 "
        "BEGIN SELECT RAISE(ABORT, 'refused'); END");

    const auto done = bucu::db::update_bookmarks(
        fixture.database, select({}),
        changes(ChangeRequest{.title = "New",
                              .tags = TagChange{.operation = TagOperation::kReplace, .text = "z"}}),
        kLater);

    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == ErrorCode::kSql);
    CHECK(cells(fixture, kAllRows) == rows);
    CHECK(cells(fixture, kAllLinks) == links);
    CHECK(cells(fixture, kAllTags) == tags);
}

TEST_CASE("update_bookmarks walks a huge range by the stored bookmarks only", "[db]") {
    Fixture fixture;
    insert_with_gaps(fixture);

    REQUIRE(bucu::db::update_bookmarks(fixture.database, select({"1-9223372036854775807"}),
                                       changes(set_title("New")), kLater)
                .has_value());

    CHECK(cells(fixture, "SELECT count(*) FROM bookmark WHERE title = 'New'") == "5");
}
