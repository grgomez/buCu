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

// Stores a bookmark with `url`, and `title` and `tags` when not empty.
void add(const Fixture& fixture, const std::string& url, const std::string& title = "",
         const std::string& tags = "", const std::string& description = "") {
    std::vector<std::string> args{"add", url};
    if (!tags.empty()) {
        args.push_back(tags);
    }
    if (!title.empty()) {
        args.insert(args.end(), {"--title", title});
    }
    if (!description.empty()) {
        args.insert(args.end(), {"--description", description});
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
    add(fixture, "https://a.example", "Pen review");
    add(fixture, "https://b.example", "Opens soon");

    const Result result = fixture.bucu({"search", "pen"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out == "1  Pen review\n   url:   https://a.example/\n");
    CHECK(result.out == record(fixture, 1));
}

TEST_CASE("search finds a keyword in the URL, title, description, or a tag", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://rust.example/");
    add(fixture, "https://b.example", "Rust");
    add(fixture, "https://c.example", "", "", "all about rust");
    add(fixture, "https://d.example", "", "rust");
    add(fixture, "https://e.example", "Go");

    const Result result = fixture.bucu({"search", "rust"});

    CHECK(result.code == 0);
    CHECK(result.out == records(fixture, {1, 2, 3, 4}));
}

TEST_CASE("search is case-insensitive by the Unicode lowercase mapping", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", "Été à Paris");

    CHECK(fixture.bucu({"search", "ÉTÉ"}).out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "paris"}).out == record(fixture, 1));
}

TEST_CASE("search ranks by keywords matched, then ID, with an empty line between records",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", "web");
    add(fixture, "https://b.example", "docs");
    add(fixture, "https://c.example", "web docs");

    const Result result = fixture.bucu({"--color", "never", "search", "web", "docs"});

    CHECK(result.code == 0);
    CHECK(result.out == read_golden("search_ranked.txt"));
    CHECK(result.out == records(fixture, {3, 1, 2}));
}

TEST_CASE("search matches whole words: e is a word character and - is not", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", "café-bar");
    add(fixture, "https://b.example", "cafébar");

    CHECK(fixture.bucu({"search", "café"}).out == record(fixture, 1));
}

TEST_CASE("search with no match, or no bookmark stored, prints nothing and exits 0",
          "[cli][search]") {
    const Fixture fixture;
    CHECK(fixture.bucu({"search", "nothing"}).out.empty());
    add(fixture, "https://a.example", "Pen");

    const Result result = fixture.bucu({"search", "nothing"});

    CHECK(result.code == 0);
    CHECK(result.out.empty());
    CHECK(result.err.empty());
}

TEST_CASE("search takes each argument as one keyword: trimmed, repeats counted once",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", "web dev");
    add(fixture, "https://b.example", "web");

    CHECK(fixture.bucu({"search", "  web  dev "}).out == record(fixture, 1));
    // The same keyword twice scores once, so the IDs keep their order.
    CHECK(fixture.bucu({"search", "web", "WEB", " web"}).out == records(fixture, {1, 2}));
    CHECK(fixture.bucu({"search", "", "web dev"}).out == record(fixture, 1));
}

TEST_CASE("search takes the words after -- as keywords, even ones that look like options",
          "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", "use --all here");
    add(fixture, "https://b.example", "web x -x");
    add(fixture, "https://c.example", "only web");

    CHECK(fixture.bucu({"search", "--", "--all"}).out == record(fixture, 1));
    CHECK(fixture.bucu({"search", "--", "-x"}).out == record(fixture, 2));
    CHECK(fixture.bucu({"search", "web", "--", "-x"}).out == records(fixture, {2, 3}));
}

TEST_CASE("search does not ask for confirmation or change the database", "[cli][search]") {
    Fixture fixture;
    add(fixture, "https://a.example", "Pen");
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
    add(fixture, "https://a.example", "Pen");

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
    add(fixture, "https://a.example", "Pen");
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
    add(fixture, "https://a.example", "Pen\x1b[31m red");

    const Result result = fixture.bucu({"search", "pen"});

    CHECK(result.code == 0);
    CHECK(result.out.find('\x1b') == std::string::npos);
    CHECK(result.out == record(fixture, 1));
}

TEST_CASE("search --color always colours the records like list", "[cli][search]") {
    const Fixture fixture;
    add(fixture, "https://a.example", "Pen");

    const Result searched = fixture.bucu({"--color", "always", "search", "pen"});
    const Result listed = fixture.bucu({"--color", "always", "list", "1"});

    CHECK(searched.code == 0);
    CHECK(searched.out == listed.out);
    CHECK(searched.out.find('\x1b') != std::string::npos);
}
