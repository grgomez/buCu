#include <sqlite3.h>

#include <sstream>
#include <string>
#include <utility>
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

namespace {

// Stores bookmarks 1 and 2 with the URLs https://a.example/ and https://b.example/.
void add_two(const Fixture& fixture) {
    REQUIRE(fixture.bucu({"add", "https://a.example", "--title", "A"}).code == 0);
    REQUIRE(fixture.bucu({"add", "https://b.example", "--title", "B"}).code == 0);
}

// `bucu update <args>` exits with `code`, prints `line` to standard error and nothing to standard
// output, and changes nothing.
void check_refused(const Fixture& fixture, const std::vector<std::string>& args, int code,
                   const std::string& line) {
    CAPTURE(args);
    const Rows bookmarks = fixture.bookmarks();
    const std::string tags = fixture.tags();
    std::vector<std::string> full = {"update"};
    full.insert(full.end(), args.begin(), args.end());

    const Result result = fixture.bucu(full);

    CHECK(result.code == code);
    CHECK(result.out.empty());
    CHECK(result.err == line);
    CHECK(fixture.bookmarks() == bookmarks);
    CHECK(fixture.tags() == tags);
}

const std::string kUsageTail = " Run 'bucu --help' for usage.\n";

// The exit code for an ID that is not stored (product section 6).
constexpr int kNotFoundCode = 5;

}  // namespace

TEST_CASE("update with no field option, or both --immutable and --no-immutable, is a usage error",
          "[cli][update]") {
    const Fixture fixture;
    add_two(fixture);
    const std::string no_field =
        "bucu: update needs at least one of --url, --title, --description, --tag, --immutable, "
        "or --no-immutable." +
        kUsageTail;

    check_refused(fixture, {"1"}, 2, no_field);
    check_refused(fixture, {"1", "--yes"}, 2, no_field);
    check_refused(fixture, {"1", "--immutable", "--no-immutable"}, 2,
                  "bucu: --immutable and --no-immutable cannot be used together." + kUsageTail);
}

TEST_CASE("update --url with a bad URL exits 3, and with another bookmark's URL exits 4",
          "[cli][update]") {
    const Fixture fixture;
    add_two(fixture);

    check_refused(fixture, {"1", "--url", "example.org"}, 3,
                  "bucu: invalid URL 'example.org': it has no scheme, such as https://.\n");
    check_refused(fixture, {"1", "--url", "HTTPS://b.example:443"}, 4,
                  "bucu: https://b.example/ is already bookmarked as 2. Use 'bucu update 2' to "
                  "change it.\n");
}

TEST_CASE("update --url with the bookmark's own URL is no conflict", "[cli][update]") {
    const Fixture fixture;
    add_two(fixture);

    const Result result = fixture.bucu({"update", "1", "--url", "https://A.example:443/"});

    CHECK(result.code == 0);
}

TEST_CASE("update of an ID that is not stored exits 5 and changes nothing", "[cli][update]") {
    const Fixture fixture;
    add_two(fixture);

    check_refused(fixture, {"42", "--title", "x"}, kNotFoundCode,
                  "bucu: no bookmark has ID 42. Run 'bucu list' to see IDs.\n");
}

TEST_CASE("update with an ID that is not a positive whole number is a usage error",
          "[cli][update]") {
    const Fixture fixture;
    add_two(fixture);

    for (const char* id : {"abc", "0", "-3", "99999999999999999999"}) {
        check_refused(fixture, {id, "--title", "x"}, 2,
                      "bucu: invalid ID '" + std::string(id) +
                          "': it is not a positive whole number." + kUsageTail);
    }
}

TEST_CASE("update reports the first problem in the order usage, validation, not found, conflict",
          "[cli][update]") {
    const Fixture fixture;
    add_two(fixture);

    // A usage error beats a validation error.
    check_refused(fixture, {"abc", "--url", "example.org"}, 2,
                  "bucu: invalid ID 'abc': it is not a positive whole number." + kUsageTail);
    check_refused(fixture, {"1,2", "--url", "example.org"}, 2,
                  "bucu: --url needs exactly one bookmark ID, because two bookmarks cannot share "
                  "a URL." +
                      kUsageTail);
    // A validation error beats not found.
    check_refused(fixture, {"42", "--url", "example.org"}, 3,
                  "bucu: invalid URL 'example.org': it has no scheme, such as https://.\n");
    // Not found beats a conflict.
    check_refused(fixture, {"42", "--url", "https://b.example"}, kNotFoundCode,
                  "bucu: no bookmark has ID 42. Run 'bucu list' to see IDs.\n");
}

TEST_CASE("--help lists exit code 5", "[cli][update]") {
    const Fixture fixture;

    const Result result = fixture.invoke({"--help"});

    CHECK(result.out.find("  5  not found: no stored bookmark matches the IDs given\n") !=
          std::string::npos);
}

namespace {

constexpr int kStoredCount = 11;

// Stores bookmarks 1 to 11 with the URLs https://<n>.example/ and the title "T<n>", then deletes
// 1, 2, 4, 6, 7, and 10, so 3, 5, 8, 9, and 11 are stored.
void add_with_gaps(const Fixture& fixture) {
    for (int n = 1; n <= kStoredCount; ++n) {
        const std::string number = std::to_string(n);
        REQUIRE(
            fixture.bucu({"add", "https://" + number + ".example", "--title", "T" + number}).code ==
            0);
    }
    exec(fixture, "DELETE FROM bookmark WHERE id IN (1, 2, 4, 6, 7, 10)");
}

std::string titles(const Fixture& fixture) {
    std::string joined;
    for (const auto& row : fixture.rows("SELECT id || '=' || title FROM bookmark ORDER BY id")) {
        joined += (joined.empty() ? "" : ",") + row[0];
    }
    return joined;
}

}  // namespace

TEST_CASE("update changes a list and a range, and prints the records in ascending ID order",
          "[cli][update]") {
    const Fixture fixture;
    add_with_gaps(fixture);

    const Result result = fixture.bucu({"update", "5,3", "11-8", "--title", "X"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out ==
          "3  X\n   url:   https://3.example/\n"
          "\n"
          "5  X\n   url:   https://5.example/\n"
          "\n"
          "8  X\n   url:   https://8.example/\n"
          "\n"
          "9  X\n   url:   https://9.example/\n"
          "\n"
          "11  X\n   url:   https://11.example/\n");
    CHECK(titles(fixture) == "3=X,5=X,8=X,9=X,11=X");
}

TEST_CASE(
    "update with a missing ID in a list, or a range with no stored ID, exits 5 and changes "
    "nothing",
    "[cli][update]") {
    const Fixture fixture;
    add_with_gaps(fixture);
    const std::string before = titles(fixture);

    const Result missing = fixture.bucu({"update", "3,4", "--title", "x"});
    const Result range = fixture.bucu({"update", "20-30", "--title", "x"});
    const Result reversed = fixture.bucu({"update", "3", "30-20", "--title", "x"});
    const Result first = fixture.bucu({"update", "12", "4", "--title", "x"});

    CHECK(missing.code == 5);
    CHECK(missing.out.empty());
    CHECK(missing.err == "bucu: no bookmark has ID 4. Run 'bucu list' to see IDs.\n");
    CHECK(range.code == 5);
    CHECK(range.out.empty());
    CHECK(range.err == "bucu: no bookmark has an ID in 20-30. Run 'bucu list' to see IDs.\n");
    CHECK(reversed.code == 5);
    CHECK(reversed.err == "bucu: no bookmark has an ID in 20-30. Run 'bucu list' to see IDs.\n");
    CHECK(first.code == 5);
    CHECK(first.err == "bucu: no bookmark has ID 4. Run 'bucu list' to see IDs.\n");
    CHECK(titles(fixture) == before);
}

TEST_CASE("update takes a range that has gaps when it holds a stored ID", "[cli][update]") {
    const Fixture fixture;
    add_with_gaps(fixture);

    const Result result = fixture.bucu({"update", "4-6", "--title", "X"});

    CHECK(result.code == 0);
    CHECK(titles(fixture) == "3=T3,5=X,8=T8,9=T9,11=T11");
}

TEST_CASE("update --url takes one distinct ID in any form, and refuses more", "[cli][update]") {
    for (const std::vector<std::string>& ids :
         {std::vector<std::string>{"5,5"}, {"5", "5"}, {"5-5"}}) {
        CAPTURE(ids);
        const Fixture fixture;
        add_with_gaps(fixture);
        std::vector<std::string> args = {"update"};
        args.insert(args.end(), ids.begin(), ids.end());
        args.insert(args.end(), {"--url", "https://new.example"});

        const Result result = fixture.bucu(args);

        CHECK(result.code == 0);
        CHECK(fixture.rows("SELECT url FROM bookmark WHERE id = 5")[0][0] ==
              "https://new.example/");
    }
    const std::string refusal =
        "bucu: --url needs exactly one bookmark ID, because two bookmarks cannot share a URL."
        " Run 'bucu --help' for usage.\n";
    for (const std::vector<std::string>& ids :
         {std::vector<std::string>{"5,6"}, {"5-6"}, {"5", "8"}, {}}) {
        CAPTURE(ids);
        const Fixture fixture;
        add_with_gaps(fixture);
        const std::string before = titles(fixture);
        std::vector<std::string> args = {"update"};
        args.insert(args.end(), ids.begin(), ids.end());
        args.insert(args.end(), {"--url", "https://new.example", "--yes"});

        const Result result = fixture.bucu(args);

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == refusal);
        CHECK(titles(fixture) == before);
    }
}

namespace {

constexpr const char* kPrompt = "Update all 3 bookmarks? [y/N] ";

// Stores bookmarks 1 to 3 with the URLs https://<n>.example/.
void add_three(const Fixture& fixture) {
    for (int n = 1; n <= 3; ++n) {
        REQUIRE(fixture.bucu({"add", "https://" + std::to_string(n) + ".example"}).code == 0);
    }
}

std::string flags(const Fixture& fixture) {
    std::string joined;
    for (const auto& row : fixture.rows("SELECT flags FROM bookmark ORDER BY id")) {
        joined += (joined.empty() ? "" : ",") + row[0];
    }
    return joined;
}

}  // namespace

TEST_CASE("update with no ID and --yes changes every bookmark and prints every record",
          "[cli][update]") {
    const Fixture fixture;
    add_three(fixture);

    const Result result = fixture.bucu({"update", "--immutable", "--yes"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out ==
          "1\n   url:   https://1.example/\n   flags: immutable-title\n"
          "\n"
          "2\n   url:   https://2.example/\n   flags: immutable-title\n"
          "\n"
          "3\n   url:   https://3.example/\n   flags: immutable-title\n");
    CHECK(flags(fixture) == "1,1,1");
}

TEST_CASE("update with no ID in a run that is not interactive needs --yes", "[cli][update]") {
    const std::string line =
        "bucu: changing all bookmarks needs confirmation; add --yes to confirm. Run 'bucu --help' "
        "for usage.\n";
    for (const bucu::platform::StandardStreams streams :
         {bucu::test::cli::kPiped,
          bucu::platform::StandardStreams{.input = bucu::platform::TerminalStatus::kTerminal,
                                          .output = bucu::platform::TerminalStatus::kNotTerminal},
          bucu::platform::StandardStreams{.input = bucu::platform::TerminalStatus::kNotTerminal,
                                          .output = bucu::platform::TerminalStatus::kTerminal}}) {
        Fixture fixture;
        add_three(fixture);
        fixture.terminals = streams;
        fixture.input = "y\n";

        const Result result = fixture.bucu({"update", "--immutable"});

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == line);
        CHECK(flags(fixture) == "0,0,0");
    }
}

TEST_CASE("update with no ID in an interactive run asks first, and y or yes in any case applies it",
          "[cli][update]") {
    for (const char* answer : {"y\n", "YES\n", " y \n", "Yes", "y"}) {
        CAPTURE(answer);
        Fixture fixture;
        add_three(fixture);
        fixture.terminals = bucu::test::cli::kTerminal;
        fixture.input = answer;

        const Result result = fixture.bucu({"update", "--immutable"});

        CHECK(result.code == 0);
        CHECK(result.err.empty());
        CHECK(result.out.starts_with(std::string(kPrompt) + "1\n"));
        CHECK(flags(fixture) == "1,1,1");
    }
}

TEST_CASE("update with no ID cancels on an empty answer, n, anything else, or end of input",
          "[cli][update]") {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"\n", std::string(kPrompt) + "Cancelled.\n"},
        {"n\n", std::string(kPrompt) + "Cancelled.\n"},
        {"yep\n", std::string(kPrompt) + "Cancelled.\n"},
        {"", std::string(kPrompt) + "\nCancelled.\n"}};
    for (const auto& [answer, expected] : cases) {
        CAPTURE(answer);
        Fixture fixture;
        add_three(fixture);
        fixture.terminals = bucu::test::cli::kTerminal;
        fixture.input = answer;

        const Result result = fixture.bucu({"update", "--immutable"});

        CHECK(result.code == 0);
        CHECK(result.err.empty());
        CHECK(result.out == expected);
        CHECK(flags(fixture) == "0,0,0");
    }
}

TEST_CASE("update with a list or a range never asks, even in an interactive run", "[cli][update]") {
    Fixture fixture;
    add_three(fixture);
    fixture.terminals = bucu::test::cli::kTerminal;

    const Result result = fixture.bucu({"update", "1,3", "--immutable"});

    CHECK(result.code == 0);
    CHECK(result.out.find("[y/N]") == std::string::npos);
    CHECK(flags(fixture) == "1,0,1");
}

TEST_CASE("update with no ID and no bookmark exits 5 before any prompt", "[cli][update]") {
    const std::string line = "bucu: there are no bookmarks.\n";
    for (const bool yes : {true, false}) {
        for (const bool interactive : {true, false}) {
            Fixture fixture;
            fixture.terminals = interactive ? bucu::test::cli::kTerminal : bucu::test::cli::kPiped;
            fixture.input = "y\n";
            std::vector<std::string> args = {"update", "--immutable"};
            if (yes) {
                args.emplace_back("--yes");
            }

            const Result result = fixture.bucu(args);

            CHECK(result.code == 5);
            CHECK(result.out.empty());
            CHECK(result.err == line);
        }
    }
}
