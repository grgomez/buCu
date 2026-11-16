#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
// rapidcheck/catch.h needs Catch2's header first.
#include <rapidcheck/catch.h>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

namespace {

using bucu::core::append_utf8;
using bucu::core::is_valid_utf8;
using bucu::core::to_lower;

// A code point from the ranges where lowercasing has work to do (Latin, Greek, Cyrillic,
// Armenian, Georgian, the case-ignorable combining marks), the sigma and dotted capital I that
// have rules of their own, or any scalar value of the Basic Multilingual Plane or beyond.
rc::Gen<char32_t> text_code_point() {
    const auto in = [](std::uint32_t first, std::uint32_t last) {
        return rc::gen::map(rc::gen::inRange<std::uint32_t>(first, last + 1),
                            [](std::uint32_t v) { return static_cast<char32_t>(v); });
    };
    return rc::gen::oneOf(in(0x20, 0x7E), in(0xC0, 0x24F), in(0x370, 0x3FF), in(0x400, 0x52F),
                          in(0x530, 0x58F), in(0x300, 0x36F), in(0x10A0, 0x10FF),
                          rc::gen::element(char32_t{0x03A3}, char32_t{0x0130}, char32_t{0x03C2}),
                          in(0x0, 0xD7FF), in(0xE000, 0x10FFFF));
}

std::string encode(const std::vector<char32_t>& code_points) {
    std::string out;
    for (const char32_t cp : code_points) {
        append_utf8(out, cp);
    }
    return out;
}

rc::Gen<std::string> valid_text() {
    return rc::gen::map(rc::gen::container<std::vector<char32_t>>(text_code_point()), encode);
}

// One row of third_party/unicode/lowercase.inc: the code point, its lowercase, and a second code
// point of the lowercase, or 0. The test reads the generated table on its own, with a linear
// search, instead of the binary search in to_lower.
struct LowerRow {
    char32_t code_point;
    char32_t first;
    char32_t second;
};

constexpr auto kRows = std::to_array<LowerRow>({
#include "lowercase.inc"
});

constexpr char32_t kCapitalSigma = 0x03A3;    // depends on its neighbours (Final_Sigma)
constexpr char32_t kDottedCapitalI = 0x0130;  // has its own example test

std::string lower_from_table(char32_t code_point) {
    std::string out;
    const auto row = std::ranges::find(kRows, code_point, &LowerRow::code_point);
    if (row == kRows.end()) {
        append_utf8(out, code_point);
        return out;
    }
    append_utf8(out, row->first);
    if (row->second != 0) {
        append_utf8(out, row->second);
    }
    return out;
}

// Any scalar value, or a code point that has a row in the table.
rc::Gen<char32_t> table_code_point() {
    const auto any =
        rc::gen::suchThat(rc::gen::map(rc::gen::inRange<std::uint32_t>(0, 0x110000),
                                       [](std::uint32_t v) { return static_cast<char32_t>(v); }),
                          [](char32_t cp) { return cp < 0xD800 || cp > 0xDFFF; });
    const auto listed = rc::gen::map(rc::gen::inRange<std::size_t>(0, kRows.size()),
                                     [](std::size_t i) { return kRows[i].code_point; });
    return rc::gen::oneOf(any, listed);
}

}  // namespace

// Product section 4 Tags: lowercase with the Unicode full mapping, like Python's str.lower().
TEST_CASE("property: lowercasing valid UTF-8 is idempotent and stays valid (product 4 Tags)",
          "[core][lower][property]") {
    rc::prop("to_lower of to_lower equals to_lower", [] {
        const std::string text = *valid_text();
        const std::string once = to_lower(text);
        RC_ASSERT(is_valid_utf8(once));
        RC_ASSERT(to_lower(once) == once);
    });
}

TEST_CASE("property: lowercasing returns malformed input unchanged (product 4 Tags)",
          "[core][lower][property]") {
    rc::prop("a stray byte makes the text come back as given", [] {
        // A continuation byte or a byte that never occurs in UTF-8, between two valid texts.
        const char stray = static_cast<char>(*rc::gen::oneOf(
            rc::gen::inRange<unsigned>(0x80, 0xC0), rc::gen::inRange<unsigned>(0xF8, 0x100)));
        const std::string text = *valid_text() + stray + *valid_text();
        RC_ASSERT(to_lower(text) == text);
    });
}

TEST_CASE("property: lowercasing one code point matches the generated table (product 4 Tags)",
          "[core][lower][property]") {
    rc::prop("to_lower of one code point is its lowercase.inc row, or itself", [] {
        const char32_t code_point = *rc::gen::suchThat(table_code_point(), [](char32_t cp) {
            return cp != kCapitalSigma && cp != kDottedCapitalI;
        });
        std::string text;
        append_utf8(text, code_point);
        RC_ASSERT(to_lower(text) == lower_from_table(code_point));
    });
}
