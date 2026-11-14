#include "support/damage.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

#include "support/temp_dir.hpp"

namespace bucu::test {

namespace {

// The first column of the first row of `sql`, as an integer.
std::int64_t query_int(sqlite3* connection, const char* sql) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(connection, sql, -1, &statement, nullptr) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        throw std::runtime_error(std::string("cannot read: ") + sql);
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
        throw std::runtime_error("cannot open the database to damage it");
    }
    std::int64_t root_page = 0;
    std::int64_t page_size = 0;
    try {
        root_page = query_int(
            connection,
            "SELECT rootpage FROM sqlite_schema WHERE name = 'sqlite_autoindex_bookmark_1'");
        page_size = query_int(connection, "PRAGMA page_size");
    } catch (...) {
        sqlite3_close(connection);
        throw;
    }
    sqlite3_close(connection);

    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp((root_page - 1) * page_size);
    const std::string zeros(64, '\0');
    file.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
    file.flush();
    if (!file) {
        throw std::runtime_error("cannot write to the database file");
    }
}

}  // namespace bucu::test
