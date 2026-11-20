#include <fstream>
#include <sstream>
#include <string>

#include "catch_amalgamated.hpp"
#include "cli/fixture.hpp"
#include "cli/run.hpp"

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
    add_numbered(fixture, 8);

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

    const Result result = fixture.bucu({"list"});

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

TEST_CASE("--help lists list", "[cli][list]") {
    const Fixture fixture;

    CHECK(fixture.invoke({"--help"}).out.find("list [ids...]") != std::string::npos);
}
