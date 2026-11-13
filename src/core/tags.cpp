#include "bucu/core/tags.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

namespace bucu::core {

namespace {

constexpr std::size_t kMaxTagLength = 128;

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
        // `collapsed` holds no White_Space but spaces, so what is left of Cc is a real control.
        for (const char32_t cp : decode_utf8(collapsed).value_or(std::u32string())) {
            if (is_control(cp)) {
                return std::unexpected(
                    ValidationError{Field::kTag, Reason::kControlCharacter, collapsed});
            }
        }
        std::string lowered = to_lower(collapsed);
        // The limit counts code points after lowercasing: U+0130 becomes two.
        if (decode_utf8(lowered)->size() > kMaxTagLength) {
            return std::unexpected(ValidationError{Field::kTag, Reason::kTooLong, lowered});
        }
        tags.insert(std::move(lowered));
    }
    return tags;
}

}  // namespace bucu::core
