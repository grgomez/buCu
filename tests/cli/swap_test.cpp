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

// Stores bookmarks 3 and 7: bookmark 3 has the title "Three", the description "d", and the tags
// a and b; bookmark 7 has the title "Seven", the flag immutable-title, and the tag c.
void add_two(const Fixture& fixture) {
    for (int n = 1; n <= 7; ++n) {
        const std::string number = std::to_string(n);
        std::vector<std::string> args = {"add", "https://" + number + ".example"};
        if (n == 3) {
            args.insert(args.end(), {"--title", "Three", "--description", "d", "--tag", "a,b"});
        } else if (n == 7) {
            args.insert(args.end(), {"--title", "Seven", "--tag", "c", "--immutable"});
        }
        REQUIRE(fixture.bucu(args).code == 0);
    }
    exec(fixture,
         "PRAGMA foreign_keys = ON; DELETE FROM bookmark WHERE id NOT IN (3, 7); "
         "DELETE FROM tag WHERE id NOT IN (SELECT tag_id FROM bookmark_tag)");
}

const std::string kUsageTail = " Run 'bucu --help' for usage.\n";

// The URL, title, and tags stored under `id`, as one text.
std::string content_of(const Fixture& fixture, int id) {
    std::string text;
    const std::string number = std::to_string(id);
    for (const auto& row :
         fixture.rows(("SELECT url || '|' || title || '|' || description || '|' || flags || '|' || "
                       "created_at || '|' || updated_at FROM bookmark WHERE id = " +
                       number)
                          .c_str())) {
        text += row[0];
    }
    for (const auto& row : fixture.rows(("SELECT tag.name FROM bookmark_tag JOIN tag ON "
                                         "tag.id = tag_id WHERE bookmark_id = " +
                                         number + " ORDER BY name")
                                            .c_str())) {
        text += "|" + row[0];
    }
    return text;
}

}  // namespace

TEST_CASE("swap exchanges two IDs, keeps everything else, and prints both records", "[cli][swap]") {
    const Fixture fixture;
    add_two(fixture);
    const std::string three = content_of(fixture, 3);
    const std::string seven = content_of(fixture, 7);

    const Result result = fixture.bucu({"swap", "7", "3"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out ==
          "3  Seven\n"
          "   url:   https://7.example/\n"
          "   tags:  c\n"
          "   flags: immutable-title\n"
          "\n"
          "7  Three\n"
          "   url:   https://3.example/\n"
          "   desc:  d\n"
          "   tags:  a, b\n");
    CHECK(content_of(fixture, 3) == seven);
    CHECK(content_of(fixture, 7) == three);
}

TEST_CASE("swap with other than two single IDs is a usage error", "[cli][swap]") {
    const Fixture fixture;
    add_two(fixture);
    const Rows before = fixture.bookmarks();

    for (const std::vector<std::string>& ids : {std::vector<std::string>{},
                                                {"3"},
                                                {"3", "7", "5"},
                                                {"abc"},
                                                {"3,3", "7"},
                                                {"3-3", "7"},
                                                {"3-4", "7"},
                                                {"3", "7,9"}}) {
        std::vector<std::string> args = {"swap"};
        args.insert(args.end(), ids.begin(), ids.end());
        CAPTURE(args);

        const Result result = fixture.bucu(args);

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == "bucu: swap takes two IDs." + kUsageTail);
    }
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("swap of an ID with itself is a usage error", "[cli][swap]") {
    const Fixture fixture;
    add_two(fixture);
    const Rows before = fixture.bookmarks();

    for (const std::vector<std::string>& ids : {std::vector<std::string>{"3", "3"}, {"3", "003"}}) {
        const Result result = fixture.bucu({"swap", ids[0], ids[1]});

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == "bucu: swap needs two different IDs." + kUsageTail);
    }
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("swap with an argument that is not an ID is the invalid ID error", "[cli][swap]") {
    const Fixture fixture;
    add_two(fixture);

    const Result result = fixture.bucu({"swap", "abc", "7"});

    CHECK(result.code == 2);
    CHECK(result.err == "bucu: invalid ID 'abc': it is not a positive whole number." + kUsageTail);

    const Result second = fixture.bucu({"swap", "3", "0"});

    CHECK(second.code == 2);
    CHECK(second.err == "bucu: invalid ID '0': it is not a positive whole number." + kUsageTail);
}

TEST_CASE("swap with an ID that is not stored is exit 5 and changes nothing", "[cli][swap]") {
    const Fixture fixture;
    add_two(fixture);
    const Rows before = fixture.bookmarks();

    for (const auto& [args, missing] :
         {std::pair<std::vector<std::string>, std::string>{{"swap", "3", "9"}, "9"},
          {{"swap", "9", "3"}, "9"},
          {{"swap", "9", "5"}, "5"}}) {
        const Result result = fixture.bucu(args);

        CHECK(result.code == 5);
        CHECK(result.out.empty());
        CHECK(result.err ==
              "bucu: no bookmark has ID " + missing + ". Run 'bucu list' to see IDs.\n");
    }
    CHECK(fixture.bookmarks() == before);
}
