#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "bucu/platform/paths.hpp"
#include "catch_amalgamated.hpp"
#include "cli/run.hpp"
#include "platform/fake_environment.hpp"
#include "support/damage.hpp"
#include "support/sqlite_read.hpp"
#include "support/temp_dir.hpp"

namespace {

struct Result {
    int code;
    std::string out;
    std::string err;
};

constexpr std::int64_t kNow = 1700000000;

// A database file in a fresh directory, and `bucu` runs against it.
struct Fixture {
    bucu::test::TempDir dir;
    bucu::platform::test::FakeEnvironment environment;
    std::filesystem::path db = dir.path() / "bookmarks.db";

    std::string db_text() const { return bucu::test::utf8_path(db); }

    // `bucu --db <db> <args>`
    Result bucu(std::vector<std::string> args) const {
        args.insert(args.begin(), {"--db", db_text()});
        return invoke(args, environment);
    }

    static Result invoke(const std::vector<std::string>& args,
                         const bucu::platform::Environment& environment) {
        const bucu::cli::Context context{bucu::platform::Os::kLinuxOrBsd, environment, kNow};
        std::ostringstream out;
        std::ostringstream err;
        const int code = bucu::cli::run(args, out, err, context);
        return {code, out.str(), err.str()};
    }

    // A file that is not a SQLite database.
    void write_text_file() const {
        std::ofstream file(db, std::ios::binary);
        file << "this is not a SQLite database, only some text that is long enough to be read\n";
    }

    // An empty database file that says its schema version is `version`, made without BuCu.
    void write_schema_version(int version) const {
        sqlite3* connection = nullptr;
        REQUIRE(sqlite3_open(db_text().c_str(), &connection) == SQLITE_OK);
        const std::string sql = "PRAGMA user_version = " + std::to_string(version);
        REQUIRE(sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_close(connection) == SQLITE_OK);
    }
};

void check_names_file(const Result& result, const Fixture& fixture) {
    CHECK(result.code == 1);
    CHECK(result.out.empty());
    CHECK(result.err.starts_with("bucu: "));
    CHECK(result.err.find(fixture.db_text()) != std::string::npos);
    CHECK(std::count(result.err.begin(), result.err.end(), '\n') == 1);
}

}  // namespace

TEST_CASE("db path prints the absolute path and creates nothing", "[cli][db]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"db", "path"});

    CHECK(result.code == 0);
    CHECK(result.out == fixture.db_text() + "\n");
    CHECK(result.err.empty());
    CHECK_FALSE(std::filesystem::exists(fixture.db));
}

TEST_CASE("db path makes a relative --db absolute and does not need its directory", "[cli][db]") {
    const Fixture fixture;
    const auto expected = std::filesystem::absolute("no-such-dir/rel.db");

    const Result result =
        Fixture::invoke({"--db", "no-such-dir/rel.db", "db", "path"}, fixture.environment);

    CHECK(result.code == 0);
    CHECK(result.out == bucu::test::utf8_path(expected) + "\n");
    CHECK_FALSE(std::filesystem::exists("no-such-dir"));
}

TEST_CASE("db path follows BUCU_DB and the default location (product section 3)", "[cli][db]") {
    const Fixture fixture;
    const bucu::platform::test::FakeEnvironment from_variable({{"BUCU_DB", fixture.db_text()}});
    // The Linux rule takes an absolute XDG_DATA_HOME that starts with "/"; the printed path is
    // made absolute for the machine that runs the test.
    const bucu::platform::test::FakeEnvironment from_default(
        {{"XDG_DATA_HOME", "/bucu-test-data"}});

    const Result variable = Fixture::invoke({"db", "path"}, from_variable);
    const Result by_default = Fixture::invoke({"db", "path"}, from_default);
    const std::filesystem::path expected = std::filesystem::absolute(
        bucu::platform::path_from_utf8("/bucu-test-data/bucu/bookmarks.db"));

    CHECK(variable.out == fixture.db_text() + "\n");
    CHECK(by_default.code == 0);
    CHECK(by_default.out == bucu::test::utf8_path(expected) + "\n");
    CHECK_FALSE(std::filesystem::exists(expected.parent_path().parent_path()));
}

TEST_CASE("db path reports a location that cannot be determined", "[cli][db]") {
    const Fixture fixture;

    const Result result = Fixture::invoke({"db", "path"}, fixture.environment);

    CHECK(result.code == 1);
    CHECK(result.out.empty());
    CHECK(result.err.starts_with("bucu: "));
    CHECK(result.err.find("HOME") != std::string::npos);
}

TEST_CASE("db version prints the schema version and the newest supported", "[cli][db]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"db", "version"});

    CHECK(result.code == 0);
    CHECK(result.out == "schema 1 (newest supported: 1)\n");
    CHECK(result.err.empty());
    CHECK(std::filesystem::exists(fixture.db));
}

TEST_CASE("db version on a file that is not a database exits 1 and names the file", "[cli][db]") {
    const Fixture fixture;
    fixture.write_text_file();

    check_names_file(fixture.bucu({"db", "version"}), fixture);
}

TEST_CASE("db version on a newer schema exits 1, names the file, and says both versions",
          "[cli][db]") {
    const Fixture fixture;
    fixture.write_schema_version(7);

    const Result result = fixture.bucu({"db", "version"});

    check_names_file(result, fixture);
    CHECK(result.err.find('7') != std::string::npos);
    CHECK(result.err.find("version 1") != std::string::npos);
}

TEST_CASE("db with no subcommand, or an unknown one, is a usage error", "[cli][db]") {
    const Fixture fixture;

    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"db"}, {"db", "bogus"}, {"db", "path", "extra"}}) {
        CAPTURE(args);
        const Result result = fixture.bucu(args);

        CHECK(result.code == 2);
        CHECK(result.out.empty());
        CHECK(result.err.starts_with("bucu: "));
        CHECK(result.err.ends_with(". Run 'bucu --help' for usage.\n"));
    }
}

TEST_CASE("db check prints ok for an intact database", "[cli][db]") {
    const Fixture fixture;
    REQUIRE(fixture.bucu({"add", "https://example.org", "web"}).code == 0);

    const Result result = fixture.bucu({"db", "check"});

    CHECK(result.code == 0);
    CHECK(result.out == "ok\n");
    CHECK(result.err.empty());
}

TEST_CASE("db check prints the report and the error line for a damaged database", "[cli][db]") {
    const Fixture fixture;
    for (const char* url :
         {"https://one.example", "https://two.example", "https://three.example"}) {
        REQUIRE(fixture.bucu({"add", url}).code == 0);
    }
    bucu::test::damage_url_index(fixture.db);

    const Result result = fixture.bucu({"db", "check"});

    CHECK(result.code == 1);
    CHECK_FALSE(result.out.empty());
    CHECK(result.out != "ok\n");
    CHECK(result.out.ends_with('\n'));
    CHECK(result.err ==
          "bucu: the database failed its integrity check. Restore a backup, or export what is "
          "readable.\n");
}

TEST_CASE("db check on a file that is not a database exits 1 and names the file", "[cli][db]") {
    const Fixture fixture;
    fixture.write_text_file();

    check_names_file(fixture.bucu({"db", "check"}), fixture);
}

TEST_CASE("db check on a newer schema exits 1 and names the file", "[cli][db]") {
    const Fixture fixture;
    fixture.write_schema_version(7);

    check_names_file(fixture.bucu({"db", "check"}), fixture);
}

namespace {

// Adds two bookmarks, one with tags, so a backup has something to hold.
void fill(const Fixture& fixture) {
    REQUIRE(fixture.bucu({"add", "https://one.example", "web", "dev", "--title", "One"}).code == 0);
    REQUIRE(fixture.bucu({"add", "https://two.example", "web"}).code == 0);
}

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

}  // namespace

TEST_CASE("db backup writes a copy that opens, checks, and holds the same data", "[cli][db]") {
    const Fixture fixture;
    fill(fixture);
    const auto copy = fixture.dir.path() / "copy.db";
    const std::string copy_text = bucu::test::utf8_path(copy);

    const Result result = fixture.bucu({"db", "backup", copy_text});

    CHECK(result.code == 0);
    CHECK(result.out ==
          "Backed up to " + copy_text + ".\nFor a portable copy, use 'bucu export'.\n");
    CHECK(result.err.empty());
    const auto version = Fixture::invoke({"--db", copy_text, "db", "version"}, fixture.environment);
    CHECK(version.out == "schema 1 (newest supported: 1)\n");
    const auto check = Fixture::invoke({"--db", copy_text, "db", "check"}, fixture.environment);
    CHECK(check.out == "ok\n");
    const char* sql =
        "SELECT bookmark.id, url, title, description, flags, created_at, updated_at, "
        "(SELECT group_concat(name) FROM (SELECT tag.name FROM bookmark_tag "
        "JOIN tag ON tag.id = tag_id WHERE bookmark_id = bookmark.id ORDER BY tag.name)) "
        "FROM bookmark ORDER BY bookmark.id";
    CHECK(bucu::test::read_rows(copy, sql) == bucu::test::read_rows(fixture.db, sql));
    CHECK(bucu::test::read_rows(copy, sql).size() == 2);
}

TEST_CASE("db backup prints the absolute path of a relative file name", "[cli][db]") {
    const Fixture fixture;
    const auto expected = std::filesystem::absolute("bucu-backup-test-relative.db");
    std::filesystem::remove(expected);

    const Result result = fixture.bucu({"db", "backup", "bucu-backup-test-relative.db"});
    const bool written = std::filesystem::exists(expected);
    std::filesystem::remove(expected);

    CHECK(result.code == 0);
    CHECK(written);
    CHECK(result.out.starts_with("Backed up to " + bucu::test::utf8_path(expected) + ".\n"));
}

TEST_CASE("db backup refuses an existing file and leaves it unchanged", "[cli][db]") {
    const Fixture fixture;
    fill(fixture);
    const auto existing = fixture.dir.path() / "existing.db";
    {
        std::ofstream file(existing, std::ios::binary);
        file << "keep me";
    }

    const Result result = fixture.bucu({"db", "backup", bucu::test::utf8_path(existing)});

    CHECK(result.code == 1);
    CHECK(result.out.empty());
    CHECK(result.err.starts_with("bucu: "));
    CHECK(result.err.find(bucu::test::utf8_path(existing)) != std::string::npos);
    CHECK(std::count(result.err.begin(), result.err.end(), '\n') == 1);
    CHECK(read_bytes(existing) == "keep me");
}

TEST_CASE("db backup to the database's own path is refused as an existing file", "[cli][db]") {
    const Fixture fixture;
    fill(fixture);

    const Result result = fixture.bucu({"db", "backup", fixture.db_text()});

    CHECK(result.code == 1);
    CHECK(result.out.empty());
    CHECK(result.err.find(fixture.db_text()) != std::string::npos);
    CHECK(fixture.bucu({"db", "check"}).out == "ok\n");
}

TEST_CASE("db backup into a missing directory exits 1, names the file, and creates nothing",
          "[cli][db]") {
    const Fixture fixture;
    const auto copy = fixture.dir.path() / "missing" / "copy.db";

    const Result result = fixture.bucu({"db", "backup", bucu::test::utf8_path(copy)});

    CHECK(result.code == 1);
    CHECK(result.out.empty());
    CHECK(result.err.find(bucu::test::utf8_path(copy)) != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(fixture.dir.path() / "missing"));
}

TEST_CASE("db backup of a file that is not a database, or a newer schema, exits 1", "[cli][db]") {
    for (const bool newer : {false, true}) {
        CAPTURE(newer);
        const Fixture fixture;
        if (newer) {
            fixture.write_schema_version(7);
        } else {
            fixture.write_text_file();
        }
        const auto copy = fixture.dir.path() / "copy.db";

        const Result result = fixture.bucu({"db", "backup", bucu::test::utf8_path(copy)});

        check_names_file(result, fixture);
        CHECK_FALSE(std::filesystem::exists(copy));
    }
}

TEST_CASE("db backup with no file is a usage error", "[cli][db]") {
    const Fixture fixture;

    const Result result = fixture.bucu({"db", "backup"});

    CHECK(result.code == 2);
    CHECK(result.out.empty());
    CHECK(result.err.ends_with(". Run 'bucu --help' for usage.\n"));
}
