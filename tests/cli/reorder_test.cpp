#include <sqlite3.h>

#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "cli/fixture.hpp"

namespace {

using bucu::test::cli::Fixture;
using bucu::test::cli::Result;
using bucu::test::cli::Rows;

// Runs SQL on the fixture's database file, as a tool other than BuCu would.
void exec(const Fixture& fixture, const char* sql) {
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open(fixture.db_text().c_str(), &connection) == SQLITE_OK);
    REQUIRE(sqlite3_exec(connection, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(connection) == SQLITE_OK);
}

// Stores bookmarks 1 to 9, each with the URL https://n.example/ and the tag t<n>, and keeps only
// those in `keep` (an SQL list such as "2, 5, 9").
void add_kept(const Fixture& fixture, const std::string& keep) {
    for (int n = 1; n <= 9; ++n) {
        const std::string number = std::to_string(n);
        REQUIRE(fixture
                    .bucu({"add", "https://" + number + ".example", "--title", "Title " + number,
                           "--tag", "t" + number})
                    .code == 0);
    }
    exec(fixture, ("PRAGMA foreign_keys = ON; DELETE FROM bookmark WHERE id NOT IN (" + keep +
                   "); DELETE FROM tag WHERE id NOT IN (SELECT tag_id FROM bookmark_tag)")
                      .c_str());
}

const std::string kUsageTail = " Run 'bucu --help' for usage.\n";

// "<id> <url> <tags>" of each bookmark, in ID order.
Rows stored(const Fixture& fixture) {
    return fixture.rows(
        "SELECT bookmark.id, url, group_concat(tag.name) FROM bookmark "
        "JOIN bookmark_tag ON bookmark_id = bookmark.id JOIN tag ON tag.id = tag_id "
        "GROUP BY bookmark.id ORDER BY bookmark.id");
}

}  // namespace

TEST_CASE("reorder asks, then renumbers to 1 to n in order and keeps every value",
          "[cli][reorder]") {
    Fixture fixture;
    add_kept(fixture, "2, 5, 9");
    fixture.terminals = bucu::test::cli::kTerminal;
    fixture.input = "y\n";

    const Result result = fixture.bucu({"reorder"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out == "Renumber 3 bookmarks? [y/N] Renumbered 3 bookmarks.\n");
    CHECK(stored(fixture) == Rows{{"1", "https://2.example/", "t2"},
                                  {"2", "https://5.example/", "t5"},
                                  {"3", "https://9.example/", "t9"}});
}

TEST_CASE("reorder counts only the bookmarks whose ID changes", "[cli][reorder]") {
    Fixture fixture;
    add_kept(fixture, "1, 2, 5");
    fixture.terminals = bucu::test::cli::kTerminal;
    fixture.input = "yes\n";

    const Result result = fixture.bucu({"reorder"});

    CHECK(result.out == "Renumber 1 bookmark? [y/N] Renumbered 1 bookmark.\n");
    CHECK(stored(fixture).back() == std::vector<std::string>{"3", "https://5.example/", "t5"});
}

TEST_CASE("reorder cancels on any other answer", "[cli][reorder]") {
    Fixture fixture;
    add_kept(fixture, "2, 5");
    fixture.terminals = bucu::test::cli::kTerminal;
    fixture.input = "n\n";
    const Rows before = fixture.bookmarks();

    const Result result = fixture.bucu({"reorder"});

    CHECK(result.code == 0);
    CHECK(result.out == "Renumber 2 bookmarks? [y/N] Cancelled.\n");
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("reorder with the IDs already 1 to n says so without asking", "[cli][reorder]") {
    for (const bool interactive : {false, true}) {
        Fixture fixture;
        add_kept(fixture, "1, 2, 3");
        fixture.terminals = interactive ? bucu::test::cli::kTerminal : bucu::test::cli::kPiped;
        const Rows before = fixture.bookmarks();

        const Result result = fixture.bucu({"reorder"});

        CHECK(result.code == 0);
        CHECK(result.out == "IDs are already 1-3.\n");
        CHECK(fixture.bookmarks() == before);
    }
}

TEST_CASE("reorder in a run that is not interactive needs --yes", "[cli][reorder]") {
    const Fixture fixture;
    add_kept(fixture, "2, 5");
    const Rows before = fixture.bookmarks();

    const Result refused = fixture.bucu({"reorder"});

    CHECK(refused.code == 2);
    CHECK(refused.out.empty());
    CHECK(refused.err ==
          "bucu: renumbering bookmarks needs confirmation; add --yes to confirm." + kUsageTail);
    CHECK(fixture.bookmarks() == before);

    const Result confirmed = fixture.bucu({"reorder", "--yes"});

    CHECK(confirmed.code == 0);
    CHECK(confirmed.out == "Renumbered 2 bookmarks.\n");
    CHECK(fixture.rows("SELECT id FROM bookmark ORDER BY id") == Rows{{"1"}, {"2"}});
}

TEST_CASE("reorder with no bookmark is exit 5", "[cli][reorder]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"reorder", "--yes"});

    CHECK(result.code == 5);
    CHECK(result.out.empty());
    CHECK(result.err == "bucu: there are no bookmarks.\n");
}

TEST_CASE("reorder takes no argument but --yes", "[cli][reorder]") {
    const Fixture fixture;
    add_kept(fixture, "2, 5");
    const Rows before = fixture.bookmarks();

    const Result result = fixture.bucu({"reorder", "3", "--yes"});

    CHECK(result.code == 2);
    CHECK(result.out.empty());
    CHECK(result.err == "bucu: reorder takes no ID." + kUsageTail);
    CHECK(fixture.bookmarks() == before);
}
