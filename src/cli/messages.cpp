#include "cli/messages.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace bucu::cli {

namespace {

std::string_view field_name(core::Field field) {
    switch (field) {
        case core::Field::kUrl:
            return "URL";
        case core::Field::kTag:
            return "tag";
        case core::Field::kTitle:
            return "title";
        case core::Field::kDescription:
            return "description";
    }
    return "";  // unreachable: every Field is handled above
}

// `text` with each control character written as an escape (`\n`, `\x1b`, `\u0085`), so a value
// echoed in an error stays on one line and sends no control byte to the terminal. Expects UTF-8.
std::string escape_controls(std::string_view text) {
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string escaped;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(text[index]);
        const bool c1 = byte == 0xC2 && index + 1 < text.size() &&
                        static_cast<unsigned char>(text[index + 1]) >= 0x80 &&
                        static_cast<unsigned char>(text[index + 1]) <= 0x9F;
        if (c1) {
            const auto next = static_cast<unsigned char>(text[++index]);
            escaped += "\\u00";
            escaped += kHex[next >> 4U];
            escaped += kHex[next & 0xFU];
        } else if (byte == '\n') {
            escaped += "\\n";
        } else if (byte == '\r') {
            escaped += "\\r";
        } else if (byte == '\t') {
            escaped += "\\t";
        } else if (byte < 0x20 || byte == 0x7F) {
            escaped += "\\x";
            escaped += kHex[byte >> 4U];
            escaped += kHex[byte & 0xFU];
        } else {
            escaped += static_cast<char>(byte);
        }
    }
    return escaped;
}

std::string_view reason_text(core::Reason reason) {
    switch (reason) {
        case core::Reason::kEmpty:
            return "it is empty";
        case core::Reason::kInvalidUtf8:
            return "it is not valid UTF-8";
        case core::Reason::kNoScheme:
            return "it has no scheme, such as https://";
        case core::Reason::kWhitespace:
            return "it contains whitespace";
        case core::Reason::kControlCharacter:
            return "it contains a control character";
        case core::Reason::kEmptyHost:
            return "it has no host";
        case core::Reason::kTooLong:
            return "it is longer than 128 characters";
    }
    return "";  // unreachable: every Reason is handled above
}

// `text` with a final period, unless it already ends with one.
std::string sentence(std::string text) {
    if (!text.ends_with('.')) {
        text.push_back('.');
    }
    return text;
}

}  // namespace

std::string validation_error_message(const core::ValidationError& error) {
    std::string line = "bucu: invalid " + std::string(field_name(error.field));
    const bool shows_value = error.field != core::Field::kTitle &&
                             error.field != core::Field::kDescription &&
                             error.reason != core::Reason::kInvalidUtf8;
    if (shows_value) {
        line += " '" + escape_controls(error.value) + "'";
    }
    return line + ": " + std::string(reason_text(error.reason)) + ".";
}

std::string duplicate_url_message(std::string_view url, std::int64_t id) {
    const std::string number = std::to_string(id);
    return "bucu: " + std::string(url) + " is already bookmarked as " + number +
           ". Use 'bucu update " + number + "' to change it.";
}

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
        case db::ErrorCode::kDestinationExists:
            break;
    }
    return line;
}

std::string location_error_message(const platform::Error& error) {
    return "bucu: " + sentence(error.message);
}

std::string backup_error_message(std::string_view file, const db::Error& error) {
    return "bucu: cannot back up to " + std::string(file) + ": " + sentence(error.message);
}

std::string sql_error_message(std::string_view action, const db::Error& error) {
    return "bucu: cannot " + std::string(action) + ": " + sentence(error.message);
}

}  // namespace bucu::cli
