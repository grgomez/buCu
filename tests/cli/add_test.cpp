#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "catch_amalgamated.hpp"
#include "cli/run.hpp"
#include "platform/fake_environment.hpp"
#include "support/sqlite_read.hpp"
#include "support/temp_dir.hpp"

namespace {

using Rows = std::vector<std::vector<std::string>>;

struct Result {
    int code;
    std::string out;
    std::string err;
};

constexpr std::int64_t kNow = 1700000000;

// A database file in a fresh directory, and `bucu` runs against it with a fixed clock.
struct Fixture {
    bucu::test::TempDir dir;
    bucu::platform::test::FakeEnvironment environment;
    std::filesystem::path db = dir.path() / "bookmarks.db";

    // `bucu --db <db> <args>`
    Result bucu(std::vector<std::string> args) const {
        args.insert(args.begin(), {"--db", bucu::test::utf8_path(db)});
        return invoke(args);
    }

    // `bucu <args>`, with no --db added.
    Result invoke(const std::vector<std::string>& args) const {
        const bucu::cli::Context context{bucu::platform::Os::kLinuxOrBsd, environment, kNow};
        std::ostringstream out;
        std::ostringstream err;
        const int code = bucu::cli::run(args, out, err, context);
        return {code, out.str(), err.str()};
    }

    Rows rows(const char* sql) const { return bucu::test::read_rows(db, sql); }

    Rows bookmarks() const {
        return rows(
            "SELECT id, url, title, description, flags, created_at, updated_at "
            "FROM bookmark ORDER BY id");
    }

    std::string tags() const {
        std::string joined;
        for (const auto& row : rows("SELECT name FROM tag ORDER BY name")) {
            joined += (joined.empty() ? "" : ",") + row[0];
        }
        return joined;
    }
};

}  // namespace

TEST_CASE("add stores the URL, tags, title, and description and prints the record", "[cli][add]") {
    const Fixture fixture;

    const Result result =
        fixture.bucu({"add", "HTTPS://Example.ORG:443", "web", "dev,", "tools", "--tag", "Docs",
                      "--title", "Example", "--description", "A page"});

    CHECK(result.code == 0);
    CHECK(result.err.empty());
    CHECK(result.out ==
          "1  Example\n"
          "   url:   https://example.org/\n"
          "   desc:  A page\n"
          "   tags:  docs, tools, web dev\n");
    CHECK(fixture.bookmarks() == Rows{{"1", "https://example.org/", "Example", "A page", "0",
                                       "1700000000", "1700000000"}});
    CHECK(fixture.tags() == "docs,tools,web dev");
}

TEST_CASE("add --immutable sets the flag, and a bare add prints only the ID and the URL",
          "[cli][add]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"add", "https://example.org", "--immutable"});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "1\n"
          "   url:   https://example.org/\n"
          "   flags: immutable-title\n");
    CHECK(fixture.bookmarks()[0][4] == "1");
    CHECK(fixture.tags().empty());
}

TEST_CASE("an empty --title stores an empty title", "[cli][add]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"add", "https://example.org", "--title", ""});

    CHECK(result.code == 0);
    CHECK(result.out == "1\n   url:   https://example.org/\n");
    CHECK(fixture.bookmarks()[0][2].empty());
}

TEST_CASE("tags from the positional words and from every --tag form one set", "[cli][add]") {
    const Fixture fixture;

    const Result result = fixture.bucu(
        {"add", "https://example.org", "b,", "a", "--tag", "A, c", "--tag", "c", "--tag=-x"});

    CHECK(result.code == 0);
    CHECK(result.out.find("   tags:  -x, a, b, c\n") != std::string::npos);
    CHECK(fixture.tags() == "-x,a,b,c");
}

TEST_CASE("every tag after -- is a positional tag, even one that starts with -", "[cli][add]") {
    const Fixture fixture;

    const Result result =
        fixture.bucu({"add", "https://example.org", "--tag", "a", "--", "-y", "z"});

    CHECK(result.code == 0);
    CHECK(fixture.tags() == "-y z,a");
}

TEST_CASE("a --tag takes one value, so words after it are positional tags", "[cli][add]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"add", "https://example.org", "--tag", "a", "b"});

    CHECK(result.code == 0);
    CHECK(fixture.tags() == "a,b");
}

TEST_CASE("tag input of only commas stores no tag", "[cli][add]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"add", "https://example.org", ",,", "--tag", " , "});

    CHECK(result.code == 0);
    CHECK(fixture.tags().empty());
}

TEST_CASE("--color and --offline before the command are accepted", "[cli][add]") {
    for (const char* mode : {"auto", "always", "never"}) {
        CAPTURE(mode);
        const Fixture fixture;

        const Result result =
            fixture.bucu({"--color", mode, "--offline", "add", "https://a.example"});

        CHECK(result.code == 0);
        CHECK(result.err.empty());
    }
}

TEST_CASE("a bad --color value, or a global option after the command, is a usage error",
          "[cli][add]") {
    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"--color", "blue", "add", "https://a.example"},
          {"add", "https://a.example", "--color", "never"},
          {"add", "https://a.example", "--offline"}}) {
        const Fixture fixture;

        const Result result = fixture.bucu(args);

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err.starts_with("bucu: "));
        CHECK(result.err.ends_with(". Run 'bucu --help' for usage.\n"));
        CHECK_FALSE(std::filesystem::exists(fixture.db));
    }
}

TEST_CASE("--db after the command is a usage error that names it", "[cli][add]") {
    const Fixture fixture;

    const Result result = fixture.invoke({"add", "https://a.example", "--db", "x.db"});

    CHECK(result.code == 2);
    CHECK(result.err.find("--db") != std::string::npos);
}

TEST_CASE("a --db file that is not a database exits 1 and names the file", "[cli][add]") {
    const Fixture fixture;
    {
        std::ofstream file(fixture.db, std::ios::binary);
        file << "this is not a SQLite database, only some text that is long enough to be read\n";
    }

    const Result result = fixture.bucu({"add", "https://example.org"});

    CHECK(result.code == 1);
    CHECK(result.out.empty());
    CHECK(result.err.starts_with("bucu: "));
    CHECK(result.err.find(bucu::test::utf8_path(fixture.db)) != std::string::npos);
}

TEST_CASE("--help lists the add options", "[cli][add]") {
    const Fixture fixture;

    const Result result = fixture.invoke({"--help"});

    for (const char* word : {"add <url> [tags...]", "--title", "--description", "--tag",
                             "--immutable", "--db", "--color", "--offline"}) {
        CAPTURE(word);
        CHECK(result.out.find(word) != std::string::npos);
    }
}
