#include "bucu/core/search.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bucu/core/bookmark.hpp"
#include "bucu/db/bookmarks.hpp"
#include "bucu/db/database.hpp"
#include "catch_amalgamated.hpp"
#include "db/handle.hpp"
#include "support/temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::test::TempDir;

constexpr std::chrono::sys_seconds kNow{std::chrono::seconds{1700000000}};

struct Fixture {
    TempDir dir;
    Database database = open_database();

    // Stores a bookmark with the URL `https://<n>.example/`, and returns its ID.
    std::int64_t add(const std::string& url, const std::string& title = "",
                     const std::string& description = "", const std::string& tags = "") {
        auto bookmark =
            bucu::core::make_bookmark({.url = bucu::core::RequiredText{url},
                                       .title = bucu::core::RequiredText{title},
                                       .description = bucu::core::RequiredText{description},
                                       .tags = tags});
        REQUIRE(bookmark.has_value());
        const auto id = bucu::db::insert_bookmark(database, *bookmark, kNow);
        REQUIRE(id.has_value());
        return id->value();
    }

    // The IDs that a search gives, in the order given.
    std::vector<std::int64_t> search(const std::vector<std::string>& keywords, bool all = false,
                                     bool deep = false) {
        const auto query = bucu::core::parse_search_query(keywords, all, deep);
        REQUIRE(query.has_value());
        std::vector<std::int64_t> ids;
        const auto done = bucu::db::for_each_search_result(
            database, *query, [&ids](bucu::core::BookmarkId id, const bucu::core::Bookmark&) {
                ids.push_back(id.value());
                return true;
            });
        REQUIRE(done.has_value());
        return ids;
    }

private:
    [[nodiscard]] Database open_database() const {
        auto opened = Database::open(dir.path() / "bookmarks.db");
        REQUIRE(opened.has_value());
        return std::move(*opened);
    }
};

using Ids = std::vector<std::int64_t>;

}  // namespace

TEST_CASE("Search visits matching bookmarks in ascending ID order", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "Rust book");
    fixture.add("https://b.example/", "Go book");
    fixture.add("https://c.example/", "More rust");
    CHECK(fixture.search({"rust"}) == Ids{1, 3});
    CHECK(fixture.search({"nothing"}).empty());
}

TEST_CASE("Search matches the URL, title, description, and each tag", "[db][search]") {
    Fixture fixture;
    fixture.add("https://rust.example/");
    fixture.add("https://b.example/", "rust");
    fixture.add("https://c.example/", "", "all about rust");
    fixture.add("https://d.example/", "", "", "rust");
    fixture.add("https://e.example/", "", "", "go,web");
    CHECK(fixture.search({"rust"}) == Ids{1, 2, 3, 4});
}

TEST_CASE("Search keeps each tag a separate field", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "", "", "web dev,docs");
    fixture.add("https://b.example/", "", "", "web,dev");
    CHECK(fixture.search({"web dev"}) == Ids{1});
    CHECK(fixture.search({"dev docs"}).empty());
    CHECK(fixture.search({"docs"}) == Ids{1});
}

TEST_CASE("Search ranks by distinct keywords matched, then by ID", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "web");
    fixture.add("https://b.example/", "docs");
    fixture.add("https://c.example/", "web docs");
    fixture.add("https://d.example/", "web", "web", "web");
    fixture.add("https://e.example/", "web docs api");
    CHECK(fixture.search({"web", "docs"}) == Ids{3, 5, 1, 2, 4});
    CHECK(fixture.search({"web", "docs", "api"}) == Ids{5, 3, 1, 2, 4});
}

TEST_CASE("Search with all keywords needs every keyword", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "web");
    fixture.add("https://b.example/", "web docs");
    fixture.add("https://c.example/", "docs", "", "web");
    CHECK(fixture.search({"web", "docs"}, true) == Ids{2, 3});
    CHECK(fixture.search({"web", "docs", "api"}, true).empty());
    CHECK(fixture.search({"web"}, true) == Ids{1, 2, 3});
}

TEST_CASE("Search matches whole words, or inside words with deep", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "Opens soon");
    fixture.add("https://b.example/", "Pen review");
    CHECK(fixture.search({"pen"}) == Ids{2});
    CHECK(fixture.search({"pen"}, false, true) == Ids{1, 2});
    CHECK(fixture.search({"pen", "soon"}, true, true) == Ids{1});
}

TEST_CASE("Search gives the bookmark with its tags", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "Pen", "about pens", "writing,tools");
    const auto query = bucu::core::parse_search_query({"pen"}, false, false);
    REQUIRE(query.has_value());
    int visits = 0;
    const auto done = bucu::db::for_each_search_result(
        fixture.database, *query,
        [&visits](bucu::core::BookmarkId id, const bucu::core::Bookmark& b) {
            ++visits;
            CHECK(id.value() == 1);
            CHECK(b.url == "https://a.example/");
            CHECK(b.title == "Pen");
            CHECK(b.description == "about pens");
            CHECK(b.tags == bucu::core::TagSet{"tools", "writing"});
            return true;
        });
    REQUIRE(done.has_value());
    CHECK(visits == 1);
}

TEST_CASE("Search stops when the visitor returns false", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "web");
    fixture.add("https://b.example/", "web");
    fixture.add("https://c.example/", "web");
    const auto query = bucu::core::parse_search_query({"web"}, false, false);
    REQUIRE(query.has_value());
    int visits = 0;
    const auto done = bucu::db::for_each_search_result(
        fixture.database, *query, [&visits](bucu::core::BookmarkId, const bucu::core::Bookmark&) {
            ++visits;
            return visits < 2;
        });
    REQUIRE(done.has_value());
    CHECK(visits == 2);
}

TEST_CASE("Search on an empty database visits nothing", "[db][search]") {
    Fixture fixture;
    CHECK(fixture.search({"web"}).empty());
}

TEST_CASE("Search leaves no SQL function on the connection", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "web");
    CHECK(fixture.search({"web"}) == Ids{1});
    CHECK(sqlite3_exec(bucu::db::detail::handle(fixture.database),
                       "SELECT bucu_keyword_score('a', 'b', 'c', 'd')", nullptr, nullptr,
                       nullptr) != SQLITE_OK);
    // A second search registers the function again.
    CHECK(fixture.search({"web"}) == Ids{1});
}

TEST_CASE("Search reads one snapshot across its passes", "[db][search]") {
    Fixture fixture;
    fixture.add("https://a.example/", "web docs");
    fixture.add("https://b.example/", "web");
    auto other = Database::open(fixture.dir.path() / "bookmarks.db");
    REQUIRE(other.has_value());
    const auto query = bucu::core::parse_search_query({"web", "docs"}, false, false);
    REQUIRE(query.has_value());
    Ids seen;
    const auto done = bucu::db::for_each_search_result(
        fixture.database, *query, [&](bucu::core::BookmarkId id, const bucu::core::Bookmark&) {
            if (seen.empty()) {
                // Another connection stores a match between the passes; this walk started
                // before it, so it must not show it.
                auto extra = bucu::core::make_bookmark(
                    {.url = bucu::core::RequiredText{"https://c.example/"},
                     .title = bucu::core::RequiredText{"web"},
                     .description = bucu::core::RequiredText{""},
                     .tags = ""});
                REQUIRE(extra.has_value());
                REQUIRE(bucu::db::insert_bookmark(*other, *extra, kNow).has_value());
            }
            seen.push_back(id.value());
            return true;
        });
    REQUIRE(done.has_value());
    CHECK(seen == Ids{1, 2});
    CHECK(fixture.search({"web"}) == Ids{1, 2, 3});
}
