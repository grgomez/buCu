#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "cli/run.hpp"
#include "platform/fake_environment.hpp"
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
    const bucu::platform::test::FakeEnvironment from_default(
        {{"XDG_DATA_HOME", fixture.dir.path().string()}});

    const Result variable = Fixture::invoke({"db", "path"}, from_variable);
    const Result by_default = Fixture::invoke({"db", "path"}, from_default);

    CHECK(variable.out == fixture.db_text() + "\n");
    CHECK(by_default.code == 0);
    CHECK(by_default.out ==
          bucu::test::utf8_path(fixture.dir.path() / "bucu" / "bookmarks.db") + "\n");
    CHECK_FALSE(std::filesystem::exists(fixture.dir.path() / "bucu"));
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
