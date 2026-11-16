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
#include "property_support.hpp"

namespace {

using bucu::core::append_utf8;
using bucu::core::is_valid_utf8;
using bucu::core::to_lower;
using bucu::core::test::code_point_in;
using bucu::core::test::encode;
using bucu::core::test::kDottedCapitalI;
using bucu::core::test::kFirstCyrillic;
using bucu::core::test::kFirstGreek;
using bucu::core::test::kFirstPrintableAscii;
using bucu::core::test::kFirstSurrogate;
using bucu::core::test::kLastGreek;
using bucu::core::test::kLastLatinExtendedB;
using bucu::core::test::kLastPrintableAscii;
using bucu::core::test::kLastSurrogate;
using bucu::core::test::kMaxScalarValue;

// Code point ranges where lowercasing has work to do, and the boundaries of the scalar values.
constexpr std::uint32_t kFirstLatinLetter = 0xC0;
constexpr std::uint32_t kFirstCombiningMark = 0x300;
constexpr std::uint32_t kLastCombiningMark = 0x36F;
constexpr std::uint32_t kLastCyrillicSupplement = 0x52F;
constexpr std::uint32_t kFirstArmenian = 0x530;
constexpr std::uint32_t kLastArmenian = 0x58F;
constexpr std::uint32_t kFirstGeorgian = 0x10A0;
constexpr std::uint32_t kLastGeorgian = 0x10FF;
constexpr char32_t kCapitalSigma = 0x03A3;  // depends on its neighbours (Final_Sigma)
constexpr char32_t kFinalSmallSigma = 0x03C2;

// Bytes that never start a code point: a continuation byte, and the bytes F8 to FF.
constexpr unsigned kFirstContinuationByte = 0x80;
constexpr unsigned kAfterContinuationBytes = 0xC0;
constexpr unsigned kFirstNeverLeadByte = 0xF8;
constexpr unsigned kAfterLastByte = 0x100;

// A code point from the ranges where lowercasing has work to do (Latin, Greek, Cyrillic,
// Armenian, Georgian, the case-ignorable combining marks), the sigma and dotted capital I that
// have rules of their own, or any scalar value of the Basic Multilingual Plane or beyond.
rc::Gen<char32_t> text_code_point() {
    return rc::gen::oneOf(code_point_in(kFirstPrintableAscii, kLastPrintableAscii),
                          code_point_in(kFirstLatinLetter, kLastLatinExtendedB),
                          code_point_in(kFirstGreek, kLastGreek),
                          code_point_in(kFirstCyrillic, kLastCyrillicSupplement),
                          code_point_in(kFirstArmenian, kLastArmenian),
                          code_point_in(kFirstCombiningMark, kLastCombiningMark),
                          code_point_in(kFirstGeorgian, kLastGeorgian),
                          rc::gen::element(kCapitalSigma, kDottedCapitalI, kFinalSmallSigma),
                          code_point_in(0, kFirstSurrogate - 1),
                          code_point_in(kLastSurrogate + 1, kMaxScalarValue));
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

std::string lower_from_table(char32_t code_point) {
    std::string out;
    for (const LowerRow& row : kRows) {
        if (row.code_point == code_point) {
            append_utf8(out, row.first);
            if (row.second != 0) {
                append_utf8(out, row.second);
            }
            return out;
        }
    }
    append_utf8(out, code_point);
    return out;
}

// Any scalar value, or a code point that has a row in the table.
rc::Gen<char32_t> table_code_point() {
    const auto any = rc::gen::suchThat(code_point_in(0, kMaxScalarValue), [](char32_t cp) {
        return cp < kFirstSurrogate || cp > kLastSurrogate;
    });
    const auto listed = rc::gen::map(rc::gen::inRange<std::size_t>(0, kRows.size()),
                                     [](std::size_t i) { return kRows.at(i).code_point; });
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
            rc::gen::inRange<unsigned>(kFirstContinuationByte, kAfterContinuationBytes),
            rc::gen::inRange<unsigned>(kFirstNeverLeadByte, kAfterLastByte)));
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
