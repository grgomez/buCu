#include "bucu/db/maintenance.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bucu/core/bookmark.hpp"
#include "bucu/db/bookmarks.hpp"
#include "bucu/db/database.hpp"
#include "catch_amalgamated.hpp"
#include "support/damage.hpp"
#include "support/sqlite_read.hpp"
#include "support/temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::test::TempDir;

// Opens `path` and stores a few bookmarks in it, then closes it.
void fill(const std::filesystem::path& path) {
    auto database = Database::open(path);
    REQUIRE(database.has_value());
    for (const char* url :
         {"https://one.example/", "https://two.example/", "https://three.example/"}) {
        auto bookmark = bucu::core::make_bookmark(url, "Title", "", "a, b");
        REQUIRE(bookmark.has_value());
        REQUIRE(bucu::db::insert_bookmark(*database, *bookmark, 1700000000).has_value());
    }
}

}  // namespace

TEST_CASE("an intact database has nothing to report", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    fill(path);
    auto database = Database::open(path);
    REQUIRE(database.has_value());

    const auto report = bucu::db::integrity_check(*database);

    REQUIRE(report.has_value());
    CHECK(report->empty());
}

TEST_CASE("an empty database has nothing to report", "[db]") {
    const TempDir dir;
    auto database = Database::open(dir.path() / "bookmarks.db");
    REQUIRE(database.has_value());

    const auto report = bucu::db::integrity_check(*database);

    REQUIRE(report.has_value());
    CHECK(report->empty());
}

TEST_CASE("a damaged database still opens and its check reports problems", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    fill(path);
    bucu::test::damage_url_index(path);
    auto database = Database::open(path);
    REQUIRE(database.has_value());

    const auto report = bucu::db::integrity_check(*database);

    REQUIRE(report.has_value());
    CHECK_FALSE(report->empty());
    for (const std::string& line : *report) {
        CHECK_FALSE(line.empty());
    }
}

namespace {

std::vector<std::filesystem::path> sibling_files(const std::filesystem::path& file) {
    std::vector<std::filesystem::path> found;
    for (const char* suffix : {"", "-journal", "-wal", "-shm"}) {
        std::filesystem::path sibling = file;
        sibling += suffix;
        if (std::filesystem::exists(sibling)) {
            found.push_back(sibling);
        }
    }
    return found;
}

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

// The bookmark URLs of the database at `path`, in order.
std::vector<std::string> urls_in(const std::filesystem::path& path) {
    std::vector<std::string> urls;
    for (const auto& row : bucu::test::read_rows(path, "SELECT url FROM bookmark ORDER BY id")) {
        urls.push_back(row[0]);
    }
    return urls;
}

}  // namespace

TEST_CASE("backup copies the committed data while another connection holds a write", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    fill(path);
    auto database = Database::open(path);
    REQUIRE(database.has_value());
    sqlite3* writer = nullptr;
    REQUIRE(sqlite3_open(bucu::test::utf8_path(path).c_str(), &writer) == SQLITE_OK);
    REQUIRE(sqlite3_exec(writer,
                         "BEGIN IMMEDIATE; INSERT INTO bookmark (url, created_at, updated_at) "
                         "VALUES ('https://uncommitted.example/', 0, 0)",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    const auto copy = dir.path() / "copy.db";

    const auto result = bucu::db::backup(*database, copy);
    sqlite3_exec(writer, "ROLLBACK", nullptr, nullptr, nullptr);
    sqlite3_close(writer);

    REQUIRE(result.has_value());
    CHECK(urls_in(copy) == std::vector<std::string>{"https://one.example/", "https://two.example/",
                                                    "https://three.example/"});
    auto opened = Database::open(copy);
    REQUIRE(opened.has_value());
    CHECK(opened->schema_version() == 1);
    const auto report = bucu::db::integrity_check(*opened);
    REQUIRE(report.has_value());
    CHECK(report->empty());
}

TEST_CASE("backup keeps the tags with their bookmarks", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    fill(path);
    auto database = Database::open(path);
    REQUIRE(database.has_value());
    const auto copy = dir.path() / "copy.db";

    REQUIRE(bucu::db::backup(*database, copy).has_value());

    const char* sql =
        "SELECT bookmark.url, tag.name FROM bookmark_tag "
        "JOIN bookmark ON bookmark.id = bookmark_id JOIN tag ON tag.id = tag_id ORDER BY 1, 2";
    CHECK(bucu::test::read_rows(copy, sql) == bucu::test::read_rows(path, sql));
    CHECK(bucu::test::read_rows(copy, sql).size() == 6);
}

TEST_CASE("backup refuses an existing file and leaves it unchanged", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    fill(path);
    auto database = Database::open(path);
    REQUIRE(database.has_value());
    const auto existing = dir.path() / "existing.db";
    {
        std::ofstream file(existing, std::ios::binary);
        file << "keep me";
    }

    const auto result = bucu::db::backup(*database, existing);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == bucu::db::ErrorCode::kDestinationExists);
    CHECK(read_bytes(existing) == "keep me");
}

TEST_CASE("backup to the database's own path is refused as an existing file", "[db]") {
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    auto database = Database::open(path);
    REQUIRE(database.has_value());

    const auto result = bucu::db::backup(*database, path);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == bucu::db::ErrorCode::kDestinationExists);
}

TEST_CASE("backup does not create a missing parent directory", "[db]") {
    const TempDir dir;
    auto database = Database::open(dir.path() / "bookmarks.db");
    REQUIRE(database.has_value());
    const auto copy = dir.path() / "missing" / "copy.db";

    const auto result = bucu::db::backup(*database, copy);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == bucu::db::ErrorCode::kCannotOpen);
    CHECK_FALSE(std::filesystem::exists(dir.path() / "missing"));
}

TEST_CASE("a backup that fails part way leaves no file and no journal, wal, or shm file", "[db]") {
    const auto name =
        GENERATE(std::filesystem::path("copy.db"), std::filesystem::path(u8"données-書籤.db"));
    const TempDir dir;
    const auto path = dir.path() / "bookmarks.db";
    {
        auto database = Database::open(path);
        REQUIRE(database.has_value());
        const std::string title(2000, 'x');
        for (int number = 0; number < 100; ++number) {
            auto bookmark = bucu::core::make_bookmark(
                "https://example.org/" + std::to_string(number), title, title, "a");
            REQUIRE(bookmark.has_value());
            REQUIRE(bucu::db::insert_bookmark(*database, *bookmark, 1700000000).has_value());
        }
    }
    auto database = Database::open(path);
    REQUIRE(database.has_value());
    // The header still promises every page; the file no longer holds them.
    std::filesystem::resize_file(path, 4096 * 4);
    const auto copy = dir.path() / name;

    const auto result = bucu::db::backup(*database, copy);

    REQUIRE_FALSE(result.has_value());
    CAPTURE(result.error().message);
    CHECK(result.error().code == bucu::db::ErrorCode::kSql);
    CHECK(sibling_files(copy).empty());
}

TEST_CASE("a missing parent directory with a non-ASCII name is reported in UTF-8", "[db]") {
    const TempDir dir;
    auto database = Database::open(dir.path() / "bookmarks.db");
    REQUIRE(database.has_value());
    const auto copy = dir.path() / std::filesystem::path(u8"données-書籤") / "copy.db";

    const auto result = bucu::db::backup(*database, copy);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().message.find("données-書籤") != std::string::npos);
}
