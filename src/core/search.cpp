#include "bucu/core/search.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"
#include "core/word_characters.hpp"

namespace bucu::core {

namespace {

// Trim White_Space from both ends and turn each run of it inside into one space.
std::string collapse_whitespace(const std::u32string& code_points) {
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

// Whether the code point that ends right before byte `pos` of valid UTF-8 `text` is a word
// character. The start of the text is not one.
bool word_character_before(std::string_view text, std::size_t pos) {
    if (pos == 0) {
        return false;
    }
    std::size_t start = pos - 1;
    while (start > 0 && is_utf8_continuation(static_cast<unsigned char>(text[start]))) {
        --start;
    }
    const auto decoded = decode_utf8(text.substr(start, pos - start));
    return decoded && decoded->size() == 1 && is_word_character(decoded->front());
}

// Whether the code point that starts at byte `pos` of valid UTF-8 `text` is a word character. The
// end of the text is not one.
bool word_character_at(std::string_view text, std::size_t pos) {
    if (pos >= text.size()) {
        return false;
    }
    std::size_t end = pos + 1;
    while (end < text.size() && is_utf8_continuation(static_cast<unsigned char>(text[end]))) {
        ++end;
    }
    const auto decoded = decode_utf8(text.substr(pos, end - pos));
    return decoded && decoded->size() == 1 && is_word_character(decoded->front());
}

// Whether `keyword` occurs in the lowercased valid UTF-8 `field`; as a whole word unless `deep`.
// Every occurrence is tried, so `pen` is found in `opens pen`. A keyword starts at a lead byte,
// so each occurrence begins on a code point boundary.
bool field_matches(std::string_view keyword, std::string_view field, bool deep) {
    if (keyword.empty()) {
        return false;
    }
    std::size_t pos = field.find(keyword);
    while (pos != std::string_view::npos) {
        if (deep || (!word_character_before(field, pos) &&
                     !word_character_at(field, pos + keyword.size()))) {
            return true;
        }
        pos = field.find(keyword, pos + 1);
    }
    return false;
}

}  // namespace

std::expected<SearchQuery, ValidationError> parse_search_query(
    const std::vector<std::string>& arguments, bool all, bool deep) {
    SearchQuery query{.keywords = {}, .all = all, .deep = deep};
    for (const std::string& argument : arguments) {
        const auto decoded = decode_utf8(argument);
        if (!decoded) {
            return std::unexpected(ValidationError{
                .field = Field::kKeyword, .reason = Reason::kInvalidUtf8, .value = argument});
        }
        std::string keyword = to_lower(collapse_whitespace(*decoded));
        if (keyword.empty() || std::ranges::contains(query.keywords, keyword)) {
            continue;
        }
        query.keywords.push_back(std::move(keyword));
    }
    if (query.keywords.empty()) {
        return std::unexpected(
            ValidationError{.field = Field::kKeyword, .reason = Reason::kEmpty, .value = ""});
    }
    return query;
}

std::size_t count_matching_keywords(const SearchQuery& query,
                                    std::span<const std::string_view> fields) {
    std::vector<std::string> lowered;
    lowered.reserve(fields.size());
    for (const std::string_view field : fields) {
        if (is_valid_utf8(field)) {
            lowered.push_back(to_lower(field));
        }
    }
    return static_cast<std::size_t>(std::ranges::count_if(query.keywords, [&](const auto& keyword) {
        return std::ranges::any_of(lowered, [&](const std::string& field) {
            return field_matches(keyword, field, query.deep);
        });
    }));
}

}  // namespace bucu::core
