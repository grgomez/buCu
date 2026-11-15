#include "support/damage.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>

#include "catch_amalgamated.hpp"
#include "support/temp_dir.hpp"

namespace bucu::test {

namespace {

// The first column of the first row of `sql` as an integer, or nothing when it cannot be read.
std::optional<std::int64_t> query_int(sqlite3* connection, const char* sql) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(connection, sql, -1, &statement, nullptr) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return std::nullopt;
    }
    const std::int64_t value = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    return value;
}

}  // namespace

void damage_url_index(const std::filesystem::path& path) {
    sqlite3* connection = nullptr;
    if (sqlite3_open_v2(utf8_path(path).c_str(), &connection, SQLITE_OPEN_READONLY, nullptr) !=
        SQLITE_OK) {
        sqlite3_close(connection);
        FAIL("cannot open the database to damage it");
    }
    const std::optional<std::int64_t> root_page =
        query_int(connection,
                  "SELECT rootpage FROM sqlite_schema WHERE name = 'sqlite_autoindex_bookmark_1'");
    const std::optional<std::int64_t> page_size = query_int(connection, "PRAGMA page_size");
    sqlite3_close(connection);
    if (!root_page.has_value() || !page_size.has_value()) {
        FAIL("cannot read the root page or the page size of the database");
    }

    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp((*root_page - 1) * *page_size);
    const std::string zeros(64, '\0');
    file.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
    file.flush();
    if (!file) {
        FAIL("cannot write to the database file");
    }
}

}  // namespace bucu::test
