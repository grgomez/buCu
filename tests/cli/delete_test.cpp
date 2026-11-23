#include <sqlite3.h>

#include <sstream>
#include <streambuf>
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

// Stores bookmarks 3, 5, and 8: bookmark n has URL https://n.example/, title "Title n", and the
// tags "all" and "t<n>", and bookmark 5 alone has the tag "a".
constexpr int kLastAdded = 8;

void add_three(const Fixture& fixture) {
    for (int n = 1; n <= kLastAdded; ++n) {
        const std::string number = std::to_string(n);
        const std::string tags = n == 5 ? "all,t5,a" : "all,t" + number;
        REQUIRE(fixture
                    .bucu({"add", "https://" + number + ".example", "--title", "Title " + number,
                           "--tag", tags})
                    .code == 0);
    }
    exec(fixture,
         "PRAGMA foreign_keys = ON; DELETE FROM bookmark WHERE id NOT IN (3, 5, 8); "
         "DELETE FROM tag WHERE id NOT IN (SELECT tag_id FROM bookmark_tag)");
}

const std::string kUsageTail = " Run 'bucu --help' for usage.\n";

std::string ids(const Fixture& fixture) {
    std::string joined;
    for (const auto& row : fixture.rows("SELECT id FROM bookmark ORDER BY id")) {
        joined += (joined.empty() ? "" : ",") + row[0];
    }
    return joined;
}

}  // namespace

TEST_CASE("delete with --yes prints the records, deletes them, and keeps the other IDs",
          "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);
    const Rows kept = fixture.rows(
        "SELECT id, url, title, flags, created_at FROM bookmark "
        "WHERE id = 5");

    const Result result = fixture.bucu({"delete", "3,8", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out ==
          "3  Title 3\n"
          "   url:   https://3.example/\n"
          "   tags:  all, t3\n"
          "\n"
          "8  Title 8\n"
          "   url:   https://8.example/\n"
          "   tags:  all, t8\n"
          "\n"
          "Deleted 2 bookmarks.\n");
    CHECK(ids(fixture) == "5");
    CHECK(fixture.rows("SELECT id, url, title, flags, created_at FROM bookmark") == kept);
    CHECK(fixture.tags() == "a,all,t5");
}

TEST_CASE("delete --color always colours the records it shows", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);

    const Result result = fixture.bucu({"--color", "always", "delete", "3", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "\x1b[96m3  \x1b[0m\x1b[92;1mTitle 3\x1b[0m\n"
          "\x1b[91m   url:   \x1b[0m\x1b[93mhttps://3.example/\x1b[0m\n"
          "\x1b[91m   tags:  \x1b[0m\x1b[94mall, t3\x1b[0m\n"
          "\n"
          "Deleted 1 bookmark.\n");
}

TEST_CASE("delete writes a control character in a shown title as an escape", "[cli][delete]") {
    const Fixture fixture;
    REQUIRE(fixture.bucu({"add", "https://a.example", "--title", "a\x1b[31mb\tc"}).code == 0);

    const Result result = fixture.bucu({"delete", "1", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "1  a\\x1b[31mb\\tc\n"
          "   url:   https://a.example/\n"
          "\n"
          "Deleted 1 bookmark.\n");
}

TEST_CASE("delete of one bookmark says bookmark in the singular", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);

    const Result result = fixture.bucu({"delete", "5", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "5  Title 5\n"
          "   url:   https://5.example/\n"
          "   tags:  a, all, t5\n"
          "\n"
          "Deleted 1 bookmark.\n");
    CHECK(ids(fixture) == "3,8");
    CHECK(fixture.tags() == "all,t3,t8");
}

TEST_CASE("delete of a range counts only the stored bookmarks", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);

    const Result result = fixture.bucu({"delete", "4-9", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.out.ends_with("\nDeleted 2 bookmarks.\n"));
    CHECK(ids(fixture) == "3");
}

TEST_CASE("delete of an ID that is not stored is exit 5 with no output and no change",
          "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);
    const Rows before = fixture.bookmarks();

    const Result single = fixture.bucu({"delete", "3,4", "--yes"});

    CHECK(single.code == 5);
    CHECK(single.out.empty());
    CHECK(single.err == "bucu: no bookmark has ID 4. Run 'bucu list' to see IDs.\n");

    const Result range = fixture.bucu({"delete", "20-30", "--yes"});

    CHECK(range.code == 5);
    CHECK(range.out.empty());
    CHECK(range.err == "bucu: no bookmark has an ID in 20-30. Run 'bucu list' to see IDs.\n");
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("delete with an invalid ID is a usage error and changes nothing", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);
    const Rows before = fixture.bookmarks();

    for (const char* argument : {"abc", "0", "3,x"}) {
        CAPTURE(argument);
        const Result result = fixture.bucu({"delete", argument, "--yes"});

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == std::string("bucu: invalid ID '") + argument +
                                "': it is not a positive whole number." + kUsageTail);
    }
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("a new bookmark after delete gets one above the highest remaining ID", "[cli][delete]") {
    const Fixture highest;
    add_three(highest);
    REQUIRE(highest.bucu({"delete", "8", "--yes"}).code == 0);

    REQUIRE(highest.bucu({"add", "https://new.example"}).code == 0);

    CHECK(ids(highest) == "3,5,6");

    const Fixture lower;
    add_three(lower);
    REQUIRE(lower.bucu({"delete", "3", "--yes"}).code == 0);
    REQUIRE(lower.bucu({"add", "https://new.example"}).code == 0);

    CHECK(ids(lower) == "5,8,9");
}

TEST_CASE("delete accepts no option but --yes", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);
    const Rows before = fixture.bookmarks();

    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"delete", "3", "--title", "x", "--yes"},
          std::vector<std::string>{"delete", "3", "--yes", "--db", "x"}}) {
        const Result result = fixture.bucu(args);

        CHECK(result.code == 2);
        CHECK(result.out.empty());
    }
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("delete deletes nothing when writing a record fails, and exits 1", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);
    const Rows before = fixture.bookmarks();
    std::istringstream in;
    std::ostringstream out;
    out.setstate(std::ios::badbit);
    std::ostringstream err;
    const bucu::cli::Context context{.os = bucu::platform::Os::kLinuxOrBsd,
                                     .environment = fixture.environment,
                                     .now = kNow,
                                     .input = in,
                                     .out = out,
                                     .err = err,
                                     .terminals = kPiped};

    const int code = bucu::cli::run({"--db", fixture.db_text(), "delete", "3", "--yes"}, context);

    CHECK(code == 1);
    CHECK(err.str() == "bucu: cannot write to standard output\n");
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("delete in a run that is not interactive needs --yes before it prints anything",
          "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);
    const Rows before = fixture.bookmarks();

    const Result result = fixture.bucu({"delete", "3"});

    CHECK(result.code == 2);
    CHECK(result.out.empty());
    CHECK(result.err ==
          "bucu: deleting bookmarks needs confirmation; add --yes to confirm." + kUsageTail);
    CHECK(fixture.bookmarks() == before);

    const Result missing = fixture.bucu({"delete", "4"});

    CHECK(missing.code == 5);
    CHECK(missing.out.empty());
}

TEST_CASE("delete asks in an interactive run and deletes on y or yes", "[cli][delete]") {
    for (const char* answer : {"y\n", "yes\n", " YES \n", "Y"}) {
        CAPTURE(answer);
        Fixture fixture;
        add_three(fixture);
        fixture.terminals = bucu::test::cli::kTerminal;
        fixture.input = answer;
        const Rows kept = fixture.rows(
            "SELECT id, url, title, description, flags, created_at, updated_at FROM bookmark "
            "WHERE id IN (3, 8) ORDER BY id");

        const Result result = fixture.bucu({"--color", "never", "delete", "5"});

        CHECK(result.code == 0);
        CHECK(result.err.empty());
        CHECK(result.out ==
              "5  Title 5\n"
              "   url:   https://5.example/\n"
              "   tags:  a, all, t5\n"
              "\n"
              "Delete 1 bookmark? [y/N] Deleted 1 bookmark.\n");
        CHECK(ids(fixture) == "3,8");
        CHECK(fixture.rows("SELECT id, url, title, description, flags, created_at, updated_at "
                           "FROM bookmark ORDER BY id") == kept);
        CHECK(fixture.tags() == "all,t3,t8");
    }
}

TEST_CASE("delete counts the bookmarks in its prompt", "[cli][delete]") {
    Fixture fixture;
    add_three(fixture);
    fixture.terminals = bucu::test::cli::kTerminal;
    fixture.input = "n\n";

    const Result result = fixture.bucu({"delete", "3,8"});

    CHECK(result.out.ends_with("\nDelete 2 bookmarks? [y/N] Cancelled.\n"));
}

TEST_CASE("delete cancels on any other answer and at the end of input", "[cli][delete]") {
    for (const char* answer : {"\n", "n\n", "yep\n", "no\n"}) {
        CAPTURE(answer);
        Fixture fixture;
        add_three(fixture);
        fixture.terminals = bucu::test::cli::kTerminal;
        fixture.input = answer;
        const Rows before = fixture.bookmarks();

        const Result result = fixture.bucu({"delete", "5"});

        CHECK(result.code == 0);
        CHECK(result.out.ends_with("\n\nDelete 1 bookmark? [y/N] Cancelled.\n"));
        CHECK(fixture.bookmarks() == before);
    }

    Fixture ended;
    add_three(ended);
    ended.terminals = bucu::test::cli::kTerminal;
    const Rows before = ended.bookmarks();

    const Result result = ended.bucu({"delete", "5"});

    CHECK(result.code == 0);
    CHECK(result.out.ends_with("\n\nDelete 1 bookmark? [y/N] \nCancelled.\n"));
    CHECK(ended.bookmarks() == before);
}

TEST_CASE("delete with --yes never asks, even in an interactive run", "[cli][delete]") {
    Fixture fixture;
    add_three(fixture);
    fixture.terminals = bucu::test::cli::kTerminal;

    const Result result = fixture.bucu({"delete", "5", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.out.find("[y/N]") == std::string::npos);
    CHECK(ids(fixture) == "3,8");
}

TEST_CASE("delete with no ID asks with the count, prints no record, and deletes everything",
          "[cli][delete]") {
    Fixture fixture;
    add_three(fixture);
    fixture.terminals = bucu::test::cli::kTerminal;
    fixture.input = "y\n";

    const Result result = fixture.bucu({"delete"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out == "Delete all 3 bookmarks? [y/N] Deleted 3 bookmarks.\n");
    CHECK(ids(fixture).empty());
    CHECK(fixture.rows("SELECT 1 FROM bookmark_tag").empty());
    CHECK(fixture.tags().empty());
}

TEST_CASE("delete with no ID says bookmark in the singular for one", "[cli][delete]") {
    Fixture fixture;
    add_three(fixture);
    REQUIRE(fixture.bucu({"delete", "3,8", "--yes"}).code == 0);
    fixture.terminals = bucu::test::cli::kTerminal;
    fixture.input = "y\n";

    const Result result = fixture.bucu({"delete"});

    CHECK(result.out == "Delete all 1 bookmark? [y/N] Deleted 1 bookmark.\n");
}

TEST_CASE("delete with no ID cancels on any other answer", "[cli][delete]") {
    Fixture fixture;
    add_three(fixture);
    fixture.terminals = bucu::test::cli::kTerminal;
    fixture.input = "n\n";

    const Result result = fixture.bucu({"delete"});

    CHECK(result.code == 0);
    CHECK(result.out == "Delete all 3 bookmarks? [y/N] Cancelled.\n");
    CHECK(ids(fixture) == "3,5,8");
}

TEST_CASE("delete --yes with no ID deletes all without asking", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);

    const Result result = fixture.bucu({"delete", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.out == "Deleted 3 bookmarks.\n");
    CHECK(ids(fixture).empty());
    CHECK(fixture.tags().empty());
}

TEST_CASE("delete with no ID and no bookmark is exit 5 before any prompt", "[cli][delete]") {
    Fixture fixture;
    fixture.terminals = bucu::test::cli::kTerminal;

    for (const Result& result : {fixture.bucu({"delete", "--yes"}), fixture.bucu({"delete"})}) {
        CHECK(result.code == 5);
        CHECK(result.out.empty());
        CHECK(result.err == "bucu: there are no bookmarks.\n");
    }
}

TEST_CASE("delete with no ID in a run that is not interactive needs --yes", "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);

    const Result result = fixture.bucu({"delete"});

    CHECK(result.code == 2);
    CHECK(result.out.empty());
    CHECK(ids(fixture) == "3,5,8");
}

namespace {

// An output buffer that refuses any write that holds "Deleted", the summary line.
class RefuseSummary : public std::stringbuf {
protected:
    std::streamsize xsputn(const char* text, std::streamsize count) override {
        if (std::string(text, static_cast<std::size_t>(count)).find("Deleted") !=
            std::string::npos) {
            return 0;
        }
        return std::stringbuf::xsputn(text, count);
    }
};

}  // namespace

TEST_CASE("delete keeps the deletion when writing the summary fails, and exits 1",
          "[cli][delete]") {
    const Fixture fixture;
    add_three(fixture);
    std::istringstream in;
    RefuseSummary buffer;
    std::ostream out(&buffer);
    std::ostringstream err;
    const bucu::cli::Context context{.os = bucu::platform::Os::kLinuxOrBsd,
                                     .environment = fixture.environment,
                                     .now = kNow,
                                     .input = in,
                                     .out = out,
                                     .err = err,
                                     .terminals = kPiped};

    const int code = bucu::cli::run({"--db", fixture.db_text(), "delete", "3", "--yes"}, context);

    CHECK(code == 1);
    CHECK(err.str() == "bucu: cannot write to standard output\n");
    CHECK(ids(fixture) == "5,8");
}
