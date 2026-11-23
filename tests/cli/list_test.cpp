#include <sqlite3.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "cli/fixture.hpp"
#include "cli/run.hpp"
#include "platform/fake_environment.hpp"

namespace {

using bucu::test::cli::Fixture;
using bucu::test::cli::kNow;
using bucu::test::cli::kPiped;
using bucu::test::cli::kTerminal;
using bucu::test::cli::Result;

// Stores bookmarks 1 to `count`; bookmark n has URL https://n.example/ and title "Title n".
void add_numbered(const Fixture& fixture, int count) {
    for (int n = 1; n <= count; ++n) {
        const std::string number = std::to_string(n);
        REQUIRE(
            fixture.bucu({"add", "https://" + number + ".example", "--title", "Title " + number})
                .code == 0);
    }
}

// The record that `list` prints for bookmark `n` of add_numbered.
std::string record(int n) {
    const std::string number = std::to_string(n);
    return number + "  Title " + number + "\n   url:   https://" + number + ".example/\n";
}

constexpr int kEightStored = 8;
constexpr int kNineStored = 9;
constexpr int kHundredStored = 100;

// Deletes the bookmarks with the IDs in `ids` ("3, 5") with SQLite directly, as a tool other than
// BuCu would.
void delete_bookmarks(const Fixture& fixture, const std::string& ids) {
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open(fixture.db_text().c_str(), &connection) == SQLITE_OK);
    const std::string sql = "DELETE FROM bookmark WHERE id IN (" + ids + ")";
    REQUIRE(sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(connection) == SQLITE_OK);
}

std::string read_golden(const std::string& name) {
    std::ifstream file(std::string(BUCU_GOLDEN_DIR) + "/" + name, std::ios::binary);
    REQUIRE(file.is_open());
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

}  // namespace

TEST_CASE("list with no ID prints every bookmark in ascending ID order, separated by an empty line",
          "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 3);

    const Result result = fixture.bucu({"list"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out == record(1) + "\n" + record(2) + "\n" + record(3));
}

TEST_CASE("list prints the bookmarks that IDs and ranges cover, in ascending ID order",
          "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, kEightStored);

    CHECK(fixture.bucu({"list", "8,3"}).out == record(3) + "\n" + record(8));
    CHECK(fixture.bucu({"list", "2-4"}).out == record(2) + "\n" + record(3) + "\n" + record(4));
    CHECK(fixture.bucu({"list", "7", "1"}).out == record(1) + "\n" + record(7));
}

TEST_CASE("list fails with exit 5, printing nothing to standard output, for a missing ID or range",
          "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 3);

    const Result single = fixture.bucu({"list", "3,4"});
    CHECK(single.code == 5);
    CHECK(single.out.empty());
    CHECK(single.err == "bucu: no bookmark has ID 4. Run 'bucu list' to see IDs.\n");

    const Result range = fixture.bucu({"list", "1", "20-30"});
    CHECK(range.code == 5);
    CHECK(range.out.empty());
    CHECK(range.err == "bucu: no bookmark has an ID in 20-30. Run 'bucu list' to see IDs.\n");
}

TEST_CASE("list fails with exit 2 for an argument that is not an ID", "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 3);

    for (const char* argument : {"abc", "0", "1,", "-3"}) {
        CAPTURE(argument);
        const Result result = fixture.bucu({"list", "--", argument});
        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err.starts_with("bucu: invalid ID"));
    }
}

TEST_CASE("list with no bookmark stored prints nothing and exits 0", "[cli][list]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"list"});

    CHECK(result.code == 0);
    CHECK(result.out.empty());
    CHECK(result.err.empty());
}

TEST_CASE("list does not ask for confirmation or change the database", "[cli][list]") {
    Fixture fixture;
    add_numbered(fixture, 2);
    const auto before = fixture.bookmarks();
    fixture.terminals = kTerminal;

    const Result result = fixture.bucu({"--color", "never", "list"});

    CHECK(result.code == 0);
    CHECK(result.out == record(1) + "\n" + record(2));
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("list accepts no option, and a global option after it is a usage error", "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 1);

    CHECK(fixture.bucu({"list", "--bogus"}).code == 2);
    CHECK(fixture.bucu({"list", "--db", "x"}).code == 2);
    CHECK(fixture.bucu({"list", "--yes"}).code == 2);
}

TEST_CASE("list exits 1 when writing standard output fails", "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 2);
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

    const int code = bucu::cli::run({"--db", fixture.db_text(), "list"}, context);

    CHECK(code == 1);
    CHECK(err.str() == "bucu: cannot write to standard output\n");
}

TEST_CASE("list prints every field kind as the plain golden file", "[cli][list]") {
    const Fixture fixture;
    REQUIRE(fixture.bucu({"add", "https://a.example"}).code == 0);
    REQUIRE(fixture
                .bucu({"add", "https://b.example", "web,dev", "--title", "Second", "--description",
                       "one\ntwo\nthree"})
                .code == 0);
    REQUIRE(
        fixture.bucu({"add", "https://c.example", "docs", "--title", "Third\nline", "--immutable"})
            .code == 0);

    const Result result = fixture.bucu({"list"});

    CHECK(result.code == 0);
    CHECK(result.out == read_golden("list_plain.txt"));
    CHECK(result.out.find('\r') == std::string::npos);
}

TEST_CASE("list --last n prints the n highest stored IDs in ascending order", "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, kNineStored);
    delete_bookmarks(fixture, "3, 5, 6, 8");  // bookmarks 1, 2, 4, 7, and 9 stay

    const Result three = fixture.bucu({"list", "--last", "3"});
    CHECK(three.code == 0);
    CHECK(three.out == record(4) + "\n" + record(7) + "\n" + record(9));

    CHECK(fixture.bucu({"list", "--last", "10"}).out ==
          record(1) + "\n" + record(2) + "\n" + record(4) + "\n" + record(7) + "\n" + record(9));
    CHECK(fixture.bucu({"list", "--last=2"}).out == record(7) + "\n" + record(9));
    CHECK(fixture.bucu({"list", "--last", "0002"}).out == record(7) + "\n" + record(9));
    CHECK(fixture.bucu({"list", "--last", "9223372036854775807"}).code == 0);
}

TEST_CASE("list --last counts stored bookmarks, not ID values", "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, kHundredStored);
    std::string middle = "2";
    for (int id = 3; id < kHundredStored; ++id) {
        middle += ", " + std::to_string(id);
    }
    delete_bookmarks(fixture, middle);  // bookmarks 1 and 100 stay

    const Result two = fixture.bucu({"list", "--last", "2"});

    CHECK(two.out == record(1) + "\n" + record(kHundredStored));
}

TEST_CASE("list --last with no bookmark stored prints nothing and exits 0", "[cli][list]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"list", "--last", "5"});

    CHECK(result.code == 0);
    CHECK(result.out.empty());
    CHECK(result.err.empty());
}

TEST_CASE("list --last fails with exit 2 for a value that is not a positive whole number",
          "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 3);

    for (const char* value : {"0", "-2", "x", "2x", "1.5", "+2", "", "99999999999999999999",
                              "9223372036854775808", "3,3", "3-3", "3,5", "3-4", " 3", "3 "}) {
        CAPTURE(value);
        const Result result = fixture.bucu({"list", "--last", value});
        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == std::string("bucu: invalid value for --last '") + value +
                                "': it is not a positive whole number. Run 'bucu --help' for "
                                "usage.\n");
    }
}

TEST_CASE("list --last escapes control characters of a bad value in the error", "[cli][list]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"list", "--last", "a\x1b[31m\tb"});

    CHECK(result.code == 2);
    CHECK(result.err ==
          "bucu: invalid value for --last 'a\\x1b[31m\\tb': it is not a positive whole number. "
          "Run 'bucu --help' for usage.\n");
}

TEST_CASE("list --last with ID arguments, or twice, or with no value, is a usage error",
          "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 3);

    const Result with_ids = fixture.bucu({"list", "--last", "2", "3"});
    CHECK(with_ids.code == 2);
    CHECK(with_ids.out.empty());
    CHECK(with_ids.err == "bucu: --last cannot be used with IDs. Run 'bucu --help' for usage.\n");

    CHECK(fixture.bucu({"list", "--last"}).code == 2);
    CHECK(fixture.bucu({"list", "--last", "1", "--last", "2"}).code == 2);
}

TEST_CASE("list reports --last with IDs before a bad --last value, and that before a bad ID",
          "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 3);
    const std::string with_ids =
        "bucu: --last cannot be used with IDs. Run 'bucu --help' for usage.\n";

    CHECK(fixture.bucu({"list", "--last", "x", "3"}).err == with_ids);
    CHECK(fixture.bucu({"list", "abc", "--last", "2"}).err == with_ids);
    CHECK(fixture.bucu({"list", "abc", "--last", "x"}).err == with_ids);
    CHECK(fixture.bucu({"list", "abc"}).err.starts_with("bucu: invalid ID 'abc'"));
}

TEST_CASE("list writes a stored escape sequence as escapes, never as raw bytes", "[cli][list]") {
    const Fixture fixture;
    REQUIRE(fixture
                .bucu({"add", "https://a.example", "--title", "a\x1b[31mb\tc", "--description",
                       "x\ry\nz"})
                .code == 0);

    const Result result = fixture.bucu({"list", "1"});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "1  a\\x1b[31mb\\tc\n"
          "   url:   https://a.example/\n"
          "   desc:  x\\ry\n"
          "          z\n");
    CHECK(result.out.find('\x1b') == std::string::npos);
}

namespace {

// The colour golden form of the record that add_numbered stores as bookmark `n`.
std::string coloured_record(int n) {
    const std::string number = std::to_string(n);
    return "\x1b[96m" + number + "  \x1b[0m\x1b[92;1mTitle " + number +
           "\x1b[0m\n\x1b[91m   url:   \x1b[0m\x1b[93mhttps://" + number + ".example/\x1b[0m\n";
}

}  // namespace

TEST_CASE("list --color always colours the records even when output is not a terminal",
          "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 2);

    const Result result = fixture.bucu({"--color", "always", "list"});

    CHECK(result.code == 0);
    CHECK(result.out == coloured_record(1) + "\n" + coloured_record(2));
}

TEST_CASE("list colours on a terminal in auto mode, unless NO_COLOR is set or colour is never",
          "[cli][list]") {
    Fixture fixture;
    add_numbered(fixture, 1);
    fixture.terminals = kTerminal;

    CHECK(fixture.bucu({"list", "1"}).out == coloured_record(1));
    CHECK(fixture.bucu({"--color", "auto", "list", "1"}).out == coloured_record(1));
    CHECK(fixture.bucu({"--color", "never", "list", "1"}).out == record(1));

    const bucu::platform::test::FakeEnvironment no_color({{"NO_COLOR", "1"}});
    const auto with_no_color = [&](std::vector<std::string> args) {
        args.insert(args.begin(), {"--db", fixture.db_text()});
        return Fixture::invoke(args, no_color, "", kTerminal);
    };
    CHECK(with_no_color({"list", "1"}).out == record(1));
    CHECK(with_no_color({"--color", "always", "list", "1"}).out == coloured_record(1));
}

TEST_CASE("list --color auto has no colour when output is not a terminal", "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 1);

    CHECK(fixture.bucu({"list", "1"}).out == record(1));
    CHECK(fixture.bucu({"--color", "auto", "list", "1"}).out == record(1));
}

TEST_CASE("list --last takes the colour rule too", "[cli][list]") {
    const Fixture fixture;
    add_numbered(fixture, 2);

    CHECK(fixture.bucu({"--color", "always", "list", "--last", "1"}).out == coloured_record(2));
}

TEST_CASE("--help lists list", "[cli][list]") {
    const Fixture fixture;

    CHECK(fixture.invoke({"--help"}).out.find("list [ids...]") != std::string::npos);
}
