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
using bucu::test::cli::Result;

// Stores a bookmark with `url`, and the `extra` fields that are not empty.
struct Extra {
    std::string title{};
    std::string tags{};
    std::string description{};
};

void add(const Fixture& fixture, const std::string& url, const Extra& extra = {}) {
    std::vector<std::string> args{"add", url};
    if (!extra.tags.empty()) {
        args.push_back(extra.tags);
    }
    if (!extra.title.empty()) {
        args.insert(args.end(), {"--title", extra.title});
    }
    if (!extra.description.empty()) {
        args.insert(args.end(), {"--description", extra.description});
    }
    REQUIRE(fixture.bucu(args).code == 0);
}

// The record that `list` prints for bookmark `id`: `search` prints the same.
std::string record(const Fixture& fixture, int id) {
    const Result result = fixture.bucu({"list", std::to_string(id)});
    REQUIRE(result.code == 0);
    return result.out;
}

// The records of the bookmarks `ids`, in that order, with one empty line between them.
std::string records(const Fixture& fixture, const std::vector<int>& ids) {
    std::string out;
    for (const int id : ids) {
        if (!out.empty()) {
            out += '\n';
        }
        out += record(fixture, id);
    }
    return out;
}

std::string read_golden(const std::string& name) {
    std::ifstream file(std::string(BUCU_GOLDEN_DIR) + "/" + name, std::ios::binary);
    REQUIRE(file.is_open());
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

}  // namespace

TEST_CASE("search prints the matching bookmark in the add format and no other", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen review"});
    add(fixture, "https://b.example", {.title = "Opens soon"});

    const Result result = fixture.bucu({"search", "pen"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out == "1  Pen review\n   url:   https://a.example/\n");
    CHECK(result.out == record(fixture, 1));
}

TEST_CASE("search finds a keyword in the URL, title, description, or a tag", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://rust.example/");
    add(fixture, "https://b.example", {.title = "Rust"});
    add(fixture, "https://c.example", {.description = "all about rust"});
    add(fixture, "https://d.example", {.tags = "rust"});
    add(fixture, "https://e.example", {.title = "Go"});

    const Result result = fixture.bucu({"search", "rust"});

    CHECK(result.code == 0);
    CHECK(result.out == records(fixture, {1, 2, 3, 4}));
}

TEST_CASE("search is case-insensitive by the Unicode lowercase mapping", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Été à Paris"});

    CHECK(fixture.bucu({"search", "ÉTÉ"}).out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "paris"}).out == record(fixture, 1));
}

TEST_CASE("search ranks by keywords matched, then ID, with an empty line between records",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "web"});
    add(fixture, "https://b.example", {.title = "docs"});
    add(fixture, "https://c.example", {.title = "web docs"});

    const Result result = fixture.bucu({"--color", "never", "search", "web", "docs"});

    CHECK(result.code == 0);
    CHECK(result.out == read_golden("search_ranked.txt"));
    CHECK(result.out == records(fixture, {3, 1, 2}));
}

TEST_CASE("search matches whole words: e is a word character and - is not", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "café-bar"});
    add(fixture, "https://b.example", {.title = "cafébar"});

    CHECK(fixture.bucu({"search", "café"}).out == record(fixture, 1));
}

TEST_CASE("search with no match, or no bookmark stored, prints nothing and exits 0",
          "[cli][search]") {
    const Fixture fixture;
    const Result empty = fixture.bucu({"search", "nothing"});
    CHECK(empty.code == 0);
    CHECK(empty.out.empty());
    CHECK(empty.err.empty());
    add(fixture, "https://a.example", {.title = "Pen"});

    const Result result = fixture.bucu({"search", "nothing"});

    CHECK(result.code == 0);
    CHECK(result.out.empty());
    CHECK(result.err.empty());
}

TEST_CASE("search takes each argument as one keyword: trimmed, repeats counted once",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "web dev"});
    add(fixture, "https://b.example", {.title = "web"});

    CHECK(fixture.bucu({"search", "  web  dev "}).out == record(fixture, 1));
    // The same keyword twice scores once, so the IDs keep their order.
    CHECK(fixture.bucu({"search", "web", "WEB", " web"}).out == records(fixture, {1, 2}));
    CHECK(fixture.bucu({"search", "", "web dev"}).out == record(fixture, 1));
}

TEST_CASE("search takes the words after -- as keywords, even ones that look like options",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "use --all here"});
    add(fixture, "https://b.example", {.title = "web x -x"});
    add(fixture, "https://c.example", {.title = "only web"});

    CHECK(fixture.bucu({"search", "--", "--all"}).out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "--", "-x"}).out == record(fixture, 2));
    CHECK(fixture.bucu({"search", "web", "--", "-x"}).out == records(fixture, {2, 3}));
}

TEST_CASE("search does not ask for confirmation or change the database", "[cli][search]") {
    Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen"});
    const auto before = fixture.bookmarks();
    fixture.terminals = bucu::test::cli::kTerminal;

    const Result result = fixture.bucu({"--color", "never", "search", "pen"});

    CHECK(result.code == 0);
    CHECK(result.out == "1  Pen\n   url:   https://a.example/\n");
    CHECK(fixture.bookmarks() == before);
}

TEST_CASE("search accepts no other option, and a global option after it is a usage error",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen"});

    for (const char* option : {"--bogus", "--yes", "--last", "--db", "-x"}) {
        CAPTURE(option);
        const Result result = fixture.bucu({"search", option, "pen"});
        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err.find("Run 'bucu --help' for usage.") != std::string::npos);
    }
}

TEST_CASE("search exits 1 when writing standard output fails", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen"});
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

    const int code = bucu::cli::run({"--db", fixture.db_text(), "search", "pen"}, context);

    CHECK(code == 1);
    CHECK(err.str() == "bucu: cannot write to standard output\n");
}

TEST_CASE("search writes control characters of a stored title as escapes", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen\x1b[31m red"});

    const Result result = fixture.bucu({"search", "pen"});

    CHECK(result.code == 0);
    CHECK(result.out.find('\x1b') == std::string::npos);
    CHECK(result.out == record(fixture, 1));
}

TEST_CASE("search --color always colours the records like list", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen"});

    const Result searched = fixture.bucu({"--color", "always", "search", "pen"});
    const Result listed = fixture.bucu({"--color", "always", "list", "1"});

    CHECK(searched.code == 0);
    CHECK(searched.out == listed.out);
    CHECK(searched.out.find('\x1b') != std::string::npos);
}

TEST_CASE("search with no keyword is a usage error: exit 2, nothing on standard output",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen"});
    const std::string message =
        "bucu: search needs at least one keyword. Run 'bucu --help' for usage.\n";

    for (const std::vector<std::string>& args : {std::vector<std::string>{"search"},
                                                 {"search", ""},
                                                 {"search", "", " ", "\t"},
                                                 {"search", "--", ""}}) {
        CAPTURE(args);
        const Result result = fixture.bucu(args);
        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err == message);
    }
}

TEST_CASE("search with no keyword fails the same on an empty database", "[cli][search]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"search"});

    CHECK(result.code == 2);
    CHECK(result.err == "bucu: search needs at least one keyword. Run 'bucu --help' for usage.\n");
}

TEST_CASE("search with a keyword that is not valid UTF-8 is a validation error: exit 3",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Pen"});

    for (const char* keyword : {"\xFF", "pen\xC3", "\xED\xA0\x80"}) {
        CAPTURE(keyword);
        const Result result = fixture.bucu({"search", "pen", keyword});
        CHECK(result.code == 3);
        CHECK(result.out.empty());
        CHECK(result.err == "bucu: invalid keyword: it is not valid UTF-8.\n");
    }
}

TEST_CASE("search reports an option error before a keyword error", "[cli][search]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"search", "--bogus", "\xFF"});

    CHECK(result.code == 2);
    CHECK(result.out.empty());
    CHECK(result.err.find("--bogus") != std::string::npos);
    CHECK(result.err.find("invalid keyword") == std::string::npos);
}

TEST_CASE("search --all prints only bookmarks that match every keyword", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "web docs"});
    add(fixture, "https://b.example", {.title = "web"});

    const Result result = fixture.bucu({"search", "--all", "web", "docs"});

    CHECK(result.code == 0);
    CHECK(result.out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "web", "docs", "--all"}).out == record(fixture, 1));
}

TEST_CASE("search --all lets each keyword match in a different field", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "web", .tags = "docs"});
    add(fixture, "https://b.example", {.title = "web"});

    CHECK(fixture.bucu({"search", "--all", "web", "docs"}).out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "--all", "web", "docs", "api"}).out.empty());
}

TEST_CASE("search --deep matches inside words", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "Opens soon"});

    CHECK(fixture.bucu({"search", "pen"}).out.empty());
    CHECK(fixture.bucu({"search", "--deep", "pen"}).out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "--deep", "--all", "pen", "soon"}).out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "--deep", "--all", "pen", "late"}).out.empty());
}

TEST_CASE("search --deep ranks by keywords matched like the whole-word search", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "opens"});
    add(fixture, "https://b.example", {.title = "opens soon"});

    CHECK(fixture.bucu({"search", "--deep", "pen", "soo"}).out == records(fixture, {2, 1}));
}

TEST_CASE("search takes --all and --deep after the command only", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", {.title = "web"});

    CHECK(fixture.bucu({"--all", "search", "web"}).code == 2);
    CHECK(fixture.bucu({"--deep", "search", "web"}).code == 2);
}
