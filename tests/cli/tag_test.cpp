#include <sqlite3.h>

#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "cli/fixture.hpp"

namespace {

using bucu::test::cli::Fixture;
using bucu::test::cli::Result;
using bucu::test::cli::Rows;

// Stores https://a.example/ with the tags a and b as bookmark 1.
void add_first(const Fixture& fixture) {
    REQUIRE(fixture.bucu({"add", "https://a.example", "--tag", "a,b", "--title", "A"}).code == 0);
}

// The tags of bookmark 1 as one text, in name order.
std::string tags_of_first(const Fixture& fixture) {
    std::string joined;
    for (const auto& row : fixture.rows("SELECT tag.name FROM bookmark_tag JOIN tag ON "
                                        "tag.id = tag_id WHERE bookmark_id = 1 ORDER BY name")) {
        joined += (joined.empty() ? "" : ",") + row[0];
    }
    return joined;
}

const std::string kUsageTail = " Run 'bucu --help' for usage.\n";

}  // namespace

TEST_CASE("tag add, remove, and replace change the tags and print the record", "[cli][tag]") {
    const Fixture fixture;
    add_first(fixture);

    const Result added = fixture.bucu({"tag", "1", "add", "c,", "D"});

    CHECK(added.code == 0);
    CHECK(added.err.empty());
    CHECK(added.out ==
          "1  A\n"
          "   url:   https://a.example/\n"
          "   tags:  a, b, c, d\n");
    CHECK(tags_of_first(fixture) == "a,b,c,d");

    const Result removed = fixture.bucu({"tag", "1", "remove", "b,", "zz"});

    CHECK(removed.code == 0);
    CHECK(removed.out.find("   tags:  a, c, d\n") != std::string::npos);
    CHECK(tags_of_first(fixture) == "a,c,d");

    const Result replaced = fixture.bucu({"tag", "1", "replace", "q"});

    CHECK(replaced.code == 0);
    CHECK(replaced.out.find("   tags:  q\n") != std::string::npos);
    CHECK(tags_of_first(fixture) == "q");

    const Result cleared = fixture.bucu({"tag", "1", "replace"});

    CHECK(cleared.code == 0);
    CHECK(cleared.out ==
          "1  A\n"
          "   url:   https://a.example/\n");
    CHECK(tags_of_first(fixture).empty());
    CHECK(fixture.tags().empty());
}

TEST_CASE("tag with no tags for add or remove, or an unknown operation, is a usage error",
          "[cli][tag]") {
    const Fixture fixture;
    add_first(fixture);
    const Rows before = fixture.bookmarks();

    const std::vector<std::pair<std::vector<std::string>, std::string>> cases = {
        {{"tag", "1", "add"}, "bucu: tag add needs at least one tag." + kUsageTail},
        {{"tag", "1", "remove"}, "bucu: tag remove needs at least one tag." + kUsageTail},
        {{"tag", "1", "rename", "x"},
         "bucu: 'rename' is not a tag operation; use add, remove, or replace." + kUsageTail},
        {{"tag", "1"}, "bucu: tag needs an operation: add, remove, or replace." + kUsageTail},
        {{"tag"}, "bucu: tag needs an operation: add, remove, or replace." + kUsageTail}};
    for (const auto& [args, line] : cases) {
        CAPTURE(args);
        const Result result = fixture.bucu(args);

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == line);
        CHECK(fixture.bookmarks() == before);
        CHECK(tags_of_first(fixture) == "a,b");
    }
}

TEST_CASE("command words after the operation are tags", "[cli][tag]") {
    const Fixture fixture;
    add_first(fixture);

    CHECK(fixture.bucu({"tag", "1", "add", "db"}).code == 0);
    CHECK(tags_of_first(fixture) == "a,b,db");

    CHECK(fixture.bucu({"tag", "1", "add", "tag", "update"}).code == 0);
    CHECK(tags_of_first(fixture) == "a,b,db,tag update");

    CHECK(fixture.bucu({"tag", "1", "remove", "tag", "update"}).code == 0);
    CHECK(tags_of_first(fixture) == "a,b,db");

    // Only the first operation word is the operation: a tag may be named like one.
    CHECK(fixture.bucu({"tag", "1", "add", "remove"}).code == 0);
    CHECK(tags_of_first(fixture) == "a,b,db,remove");
}

TEST_CASE("a tag that starts with - goes after --", "[cli][tag]") {
    const Fixture fixture;
    add_first(fixture);

    const Result result = fixture.bucu({"tag", "1", "add", "--", "-x"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(tags_of_first(fixture) == "-x,a,b");
}

TEST_CASE("--help lists tag", "[cli][tag]") {
    const Fixture fixture;

    const Result result = fixture.invoke({"--help"});

    CHECK(result.out.find("tag [ids...] <operation> [tags...]") != std::string::npos);
}

namespace {

// Stores bookmarks 1 to 11 with the URLs https://<n>.example/, then deletes 1, 2, 4, 6, 7, and 10,
// so 3, 5, 8, 9, and 11 are stored.
void add_with_gaps(const Fixture& fixture) {
    for (int n = 1; n <= 11; ++n) {
        REQUIRE(fixture.bucu({"add", "https://" + std::to_string(n) + ".example"}).code == 0);
    }
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open(fixture.db_text().c_str(), &connection) == SQLITE_OK);
    REQUIRE(sqlite3_exec(connection, "DELETE FROM bookmark WHERE id IN (1, 2, 4, 6, 7, 10)",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(connection) == SQLITE_OK);
}

}  // namespace

TEST_CASE("tag changes a list and a range, and prints the records in ascending ID order",
          "[cli][tag]") {
    const Fixture fixture;
    add_with_gaps(fixture);

    const Result result = fixture.bucu({"tag", "3,5", "11-8", "add", "x"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out ==
          "3\n   url:   https://3.example/\n   tags:  x\n"
          "\n"
          "5\n   url:   https://5.example/\n   tags:  x\n"
          "\n"
          "8\n   url:   https://8.example/\n   tags:  x\n"
          "\n"
          "9\n   url:   https://9.example/\n   tags:  x\n"
          "\n"
          "11\n   url:   https://11.example/\n   tags:  x\n");
    CHECK(fixture.rows("SELECT bookmark_id FROM bookmark_tag ORDER BY bookmark_id") ==
          Rows{{"3"}, {"5"}, {"8"}, {"9"}, {"11"}});
}

TEST_CASE("tag with a missing ID, or a range with no stored ID, exits 5 and changes nothing",
          "[cli][tag]") {
    const Fixture fixture;
    add_with_gaps(fixture);

    const Result missing = fixture.bucu({"tag", "3,4", "add", "x"});
    const Result range = fixture.bucu({"tag", "20-30", "replace", "x"});

    CHECK(missing.code == 5);
    CHECK(missing.out.empty());
    CHECK(missing.err == "bucu: no bookmark has ID 4. Run 'bucu list' to see IDs.\n");
    CHECK(range.code == 5);
    CHECK(range.err == "bucu: no bookmark has an ID in 20-30. Run 'bucu list' to see IDs.\n");
    CHECK(fixture.tags().empty());
}
