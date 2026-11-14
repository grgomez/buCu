#include "bucu/db/maintenance.hpp"

#include <string>
#include <utility>

#include "bucu/core/bookmark.hpp"
#include "bucu/db/bookmarks.hpp"
#include "bucu/db/database.hpp"
#include "catch_amalgamated.hpp"
#include "support/damage.hpp"
#include "support/temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::test::TempDir;

// Opens `path` and stores a few bookmarks in it, then closes it.
void fill(const std::filesystem::path& path) {
    auto database = Database::open(path);
    REQUIRE(database.has_value());
    for (const char* url :
         {"https://one.example/", "https://two.example/", "https://three.example/"}) {
        auto bookmark = bucu::core::make_bookmark(url, "Title", "", "a, b");
        REQUIRE(bookmark.has_value());
        REQUIRE(bucu::db::insert_bookmark(*database, *bookmark, 1700000000).has_value());
    }
}

}  // namespace

TEST_CASE("an intact database has nothing to report", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    fill(path);
    auto database = Database::open(path);
    REQUIRE(database.has_value());

    const auto report = bucu::db::integrity_check(*database);

    REQUIRE(report.has_value());
    CHECK(report->empty());
}

TEST_CASE("an empty database has nothing to report", "[db]") {
    const TempDir dir;
    auto database = Database::open(dir.path() / "bookmarks.db");
    REQUIRE(database.has_value());

    const auto report = bucu::db::integrity_check(*database);

    REQUIRE(report.has_value());
    CHECK(report->empty());
}

TEST_CASE("a damaged database still opens and its check reports problems", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    fill(path);
    bucu::test::damage_url_index(path);
    auto database = Database::open(path);
    REQUIRE(database.has_value());

    const auto report = bucu::db::integrity_check(*database);

    REQUIRE(report.has_value());
    CHECK_FALSE(report->empty());
    for (const std::string& line : *report) {
        CHECK_FALSE(line.empty());
    }
}
