#include "cli/messages.hpp"

namespace bucu::cli {

namespace {

// `text` with a final period, unless it already ends with one.
std::string sentence(std::string text) {
    if (!text.ends_with('.')) {
        text.push_back('.');
    }
    return text;
}

}  // namespace

std::string open_error_message(std::string_view file, const db::Error& error) {
    std::string line = "bucu: cannot open " + std::string(file) + ": " + sentence(error.message);
    switch (error.code) {
        case db::ErrorCode::kNotADatabase:
        case db::ErrorCode::kNotBucuDatabase:
            line += " Name a BuCu database with --db.";
            break;
        case db::ErrorCode::kNewerSchema:
            line += " Update BuCu, or name another database with --db.";
            break;
        case db::ErrorCode::kCannotOpen:
        case db::ErrorCode::kMigrationFailed:
        case db::ErrorCode::kSql:
        case db::ErrorCode::kDuplicateUrl:
            break;
    }
    return line;
}

std::string location_error_message(const platform::Error& error) {
    return "bucu: " + sentence(error.message);
}

std::string sql_error_message(std::string_view action, const db::Error& error) {
    return "bucu: cannot " + std::string(action) + ": " + sentence(error.message);
}

}  // namespace bucu::cli
