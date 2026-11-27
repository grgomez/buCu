// Fuzz target for search in core: parse_search_query and count_matching_keywords.
// The input is split at NUL bytes into pieces. The low bits of the first byte choose --all and
// how many of the pieces are keyword arguments (the deep and whole-word counts are both checked);
// the other pieces are bookmark fields. Besides the sanitizer reports, it aborts when a parsed
// keyword breaks the rules of product section 5 (search), or when the whole-word count is above the
// deep count.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "bucu/core/search.hpp"
#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

namespace {

constexpr unsigned kAllBit = 1U;
constexpr unsigned kKeywordCountShift = 2;
constexpr unsigned kKeywordCountMask = 3U;

std::vector<std::string_view> split_at_nul(std::string_view text) {
    std::vector<std::string_view> pieces;
    std::size_t start = 0;
    while (true) {
        const std::size_t nul = text.find('\0', start);
        if (nul == std::string_view::npos) {
            pieces.push_back(text.substr(start));
            return pieces;
        }
        pieces.push_back(text.substr(start, nul - start));
        start = nul + 1;
    }
}

// A parsed keyword is non-empty, valid UTF-8, lowercase, and has no leading or trailing white
// space and no white space but single spaces.
bool keyword_is_well_formed(const std::string& keyword) {
    const auto decoded = bucu::core::decode_utf8(keyword);
    if (!decoded || decoded->empty() || bucu::core::to_lower(keyword) != keyword) {
        return false;
    }
    if (bucu::core::is_white_space(decoded->front()) ||
        bucu::core::is_white_space(decoded->back())) {
        return false;
    }
    bool previous_space = false;
    for (const char32_t cp : *decoded) {
        if (bucu::core::is_white_space(cp) && (cp != U' ' || previous_space)) {
            return false;
        }
        previous_space = cp == U' ';
    }
    return true;
}

}  // namespace

// libFuzzer requires this exact entry-point name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }
    const unsigned control = data[0];
    // libFuzzer passes bytes as uint8_t; std::string_view takes char.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string_view whole_input(reinterpret_cast<const char*>(data), size);
    const std::string_view text = whole_input.substr(1);
    const std::vector<std::string_view> pieces = split_at_nul(text);
    const std::size_t keyword_count =
        (((control >> kKeywordCountShift) & kKeywordCountMask) % pieces.size()) + 1;
    const auto split = std::next(pieces.begin(), static_cast<std::ptrdiff_t>(keyword_count));
    const std::vector<std::string> arguments(pieces.begin(), split);
    const std::vector<std::string_view> fields(split, pieces.end());

    const bool all = (control & kAllBit) != 0;
    const auto whole = bucu::core::parse_search_query(arguments, all, false);
    const auto deep = bucu::core::parse_search_query(arguments, all, true);
    if (whole.has_value() != deep.has_value()) {
        std::abort();
    }
    if (!whole) {
        return 0;
    }
    for (std::size_t i = 0; i < whole->keywords.size(); ++i) {
        if (!keyword_is_well_formed(whole->keywords[i])) {
            std::abort();
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (whole->keywords[i] == whole->keywords[j]) {
                std::abort();
            }
        }
    }
    const std::size_t whole_count = bucu::core::count_matching_keywords(*whole, fields);
    const std::size_t deep_count = bucu::core::count_matching_keywords(*deep, fields);
    if (whole_count > deep_count || deep_count > whole->keywords.size()) {
        std::abort();
    }
    return 0;
}
