#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "support/temp_dir.hpp"

namespace bucu::test {

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
        std::vector<std::string> row;
        for (int column = 0; column < sqlite3_column_count(statement); ++column) {
            const unsigned char* text = sqlite3_column_text(statement, column);
            row.emplace_back(text == nullptr ? "" : reinterpret_cast<const char*>(text));
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(statement);
    sqlite3_close(connection);
    return rows;
}

}  // namespace bucu::test
