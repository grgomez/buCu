#pragma once

#include <sqlite3.h>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "support/temp_dir.hpp"

namespace bucu::test {

// Column `column` of the current row as text (NULL as empty).
inline std::string column_text(sqlite3_stmt* statement, int column) {
    const unsigned char* text = sqlite3_column_text(statement, column);
    const int size = sqlite3_column_bytes(statement, column);
    const std::span<const unsigned char> bytes(text, static_cast<std::size_t>(size));
    return std::string(bytes.begin(), bytes.end());
}

// Every row of `sql` on the database at `path`, each column as text (NULL as empty). Opens its own
// read-only connection, so a test sees what a command left in the file.
inline std::vector<std::vector<std::string>> read_rows(const std::filesystem::path& path,
                                                       const char* sql) {
    sqlite3* connection = nullptr;
    REQUIRE(sqlite3_open_v2(utf8_path(path).c_str(), &connection, SQLITE_OPEN_READONLY, nullptr) ==
            SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(connection, sql, -1, &statement, nullptr) == SQLITE_OK);
    std::vector<std::vector<std::string>> rows;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int columns = sqlite3_column_count(statement);
        std::vector<std::string> row;
        row.reserve(static_cast<std::size_t>(columns));
        for (int column = 0; column < columns; ++column) {
            row.push_back(column_text(statement, column));
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(statement);
    sqlite3_close(connection);
    return rows;
}

}  // namespace bucu::test
