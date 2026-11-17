#include <sqlite3.h>

#include <sstream>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "cli/fixture.hpp"
#include "cli/run.hpp"

namespace {

using bucu::test::cli::Fixture;
using bucu::test::cli::kNow;
using bucu::test::cli::kPiped;
using bucu::test::cli::Result;
using bucu::test::cli::Rows;

// Runs SQL on the fixture's database file, as a tool other than BuCu would.
void exec(const Fixture& fixture, const char* sql) {
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open(fixture.db_text().c_str(), &connection) == SQLITE_OK);
    REQUIRE(sqlite3_exec(connection, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(connection) == SQLITE_OK);
}

// Stores https://old.example/ with title "Old", description "Words", and tags a and b as
// bookmark 1.
void add_first(const Fixture& fixture) {
    REQUIRE(fixture
                .bucu({"add", "https://old.example", "a,", "b", "--title", "Old", "--description",
                       "Words"})
                .code == 0);
}

}  // namespace

TEST_CASE("update changes every given field, keeps the tags, and prints the record",
          "[cli][update]") {
    const Fixture fixture;
    add_first(fixture);

    const Result result = fixture.bucu({"update", "1", "--url", "HTTPS://Example.ORG:443/x",
                                        "--title", "T", "--description", "D", "--immutable"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out ==
          "1  T\n"
          "   url:   https://example.org/x\n"
          "   desc:  D\n"
          "   tags:  a, b\n"
          "   flags: immutable-title\n");
    CHECK(fixture.bookmarks() ==
          Rows{{"1", "https://example.org/x", "T", "D", "1", "1700000000", "1700000000"}});
    CHECK(fixture.tags() == "a,b");
}

TEST_CASE("update with an empty title or description clears it", "[cli][update]") {
    const Fixture fixture;
    add_first(fixture);

    const Result result = fixture.bucu({"update", "1", "--title", "", "--description", ""});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "1\n"
          "   url:   https://old.example/\n"
          "   tags:  a, b\n");
    CHECK(fixture.bookmarks()[0][2].empty());
    CHECK(fixture.bookmarks()[0][3].empty());
}

TEST_CASE(
    "update --tag replaces the tags with the union of its values, and an empty one clears them",
    "[cli][update]") {
    const Fixture fixture;
    add_first(fixture);

    const Result replaced = fixture.bucu({"update", "1", "--tag", "x,y", "--tag", "Z"});

    CHECK(replaced.code == 0);
    CHECK(replaced.out.find("   tags:  x, y, z\n") != std::string::npos);
    CHECK(fixture.tags() == "x,y,z");

    const Result cleared = fixture.bucu({"update", "1", "--tag", ""});

    CHECK(cleared.code == 0);
    CHECK(cleared.out.find("tags:") == std::string::npos);
    CHECK(fixture.tags().empty());
}

TEST_CASE("update --no-immutable clears the flag and keeps the other bits", "[cli][update]") {
    const Fixture fixture;
    add_first(fixture);
    exec(fixture, "UPDATE bookmark SET flags = 5");

    const Result result = fixture.bucu({"update", "1", "--no-immutable"});

    CHECK(result.code == 0);
    CHECK(result.out.find("flags:") == std::string::npos);
    CHECK(fixture.bookmarks()[0][4] == "4");
}

TEST_CASE("update --immutable keeps the other flag bits", "[cli][update]") {
    const Fixture fixture;
    add_first(fixture);
    exec(fixture, "UPDATE bookmark SET flags = 4");

    const Result result = fixture.bucu({"update", "1", "--immutable"});

    CHECK(result.code == 0);
    CHECK(fixture.bookmarks()[0][4] == "5");
}

TEST_CASE("update with values the bookmark already has still prints the record and exits 0",
          "[cli][update]") {
    const Fixture fixture;
    add_first(fixture);
    const Rows before = fixture.bookmarks();

    const Result result = fixture.bucu({"update", "1", "--title", "Old", "--tag", "b,a"});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "1  Old\n"
          "   url:   https://old.example/\n"
          "   desc:  Words\n"
          "   tags:  a, b\n");
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("update keeps the change when writing standard output fails, and exits 1",
          "[cli][update]") {
    const Fixture fixture;
    add_first(fixture);
    std::istringstream in;
    const bucu::cli::Context context{.os = bucu::platform::Os::kLinuxOrBsd,
                                     .environment = fixture.environment,
                                     .now = kNow,
                                     .input = in,
                                     .terminals = kPiped};
    std::ostringstream out;
    out.setstate(std::ios::badbit);
    std::ostringstream err;

    const int code = bucu::cli::run({"--db", fixture.db_text(), "update", "1", "--title", "New"},
                                    out, err, context);

    CHECK(code == 1);
    CHECK(err.str() == "bucu: cannot write to standard output\n");
    CHECK(fixture.bookmarks()[0][2] == "New");
}

TEST_CASE("--help lists update", "[cli][update]") {
    const Fixture fixture;

    const Result result = fixture.invoke({"--help"});

    for (const char* word : {"update [ids...]", "--url", "--no-immutable", "--yes"}) {
        CAPTURE(word);
        CHECK(result.out.find(word) != std::string::npos);
    }
}
