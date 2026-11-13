#include "bucu/core/tags.hpp"

#include <cstddef>
#include <string>
#include <string_view>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

namespace bucu::core {

namespace {

// Trim White_Space from both ends, and turn each run of it inside into one space (D4: this runs
// before any check, so a tab or newline inside a tag becomes a space).
std::string collapse_whitespace(std::u32string_view code_points) {
    std::string out;
    bool pending_space = false;
    for (const char32_t cp : code_points) {
        if (is_white_space(cp)) {
            pending_space = !out.empty();
            continue;
        }
        if (pending_space) {
            out += ' ';
            pending_space = false;
        }
        append_utf8(out, cp);
    }
    return out;
}

}  // namespace

std::expected<TagSet, ValidationError> parse_tags(std::string_view input) {
    TagSet tags;
    std::size_t start = 0;
    while (start <= input.size()) {
        // A comma byte never occurs inside a UTF-8 sequence, so splitting on bytes is safe even
        // before the segment is validated.
        std::size_t comma = input.find(',', start);
        if (comma == std::string_view::npos) {
            comma = input.size();
        }
        const std::string_view segment = input.substr(start, comma - start);
        start = comma + 1;
        const auto decoded = decode_utf8(segment);
        if (!decoded) {
            return std::unexpected(
                ValidationError{Field::kTag, Reason::kInvalidUtf8, std::string(segment)});
        }
        const std::string collapsed = collapse_whitespace(*decoded);
        if (collapsed.empty()) {
            continue;
        }
        tags.insert(to_lower(collapsed));
    }
    return tags;
}

}  // namespace bucu::core
