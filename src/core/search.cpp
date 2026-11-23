#include "bucu/core/search.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

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

}  // namespace bucu::core
