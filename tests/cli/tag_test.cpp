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
