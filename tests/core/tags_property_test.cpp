#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bucu/core/tags.hpp"
#include "catch_amalgamated.hpp"
// rapidcheck/catch.h needs Catch2's header first.
#include <rapidcheck/catch.h>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

namespace {

using bucu::core::append_utf8;
using bucu::core::decode_utf8;
using bucu::core::is_control;
using bucu::core::is_white_space;
using bucu::core::parse_tags;
using bucu::core::TagSet;
using bucu::core::to_lower;

// The most code points a tag may have (product section 4 Tags), pinned here on purpose.
constexpr std::size_t kTagLimit = 128;

// How large the generated words, tags, and inputs get (the generator size, not a rule).
constexpr int kWordSize = 8;
constexpr int kWordsPerTag = 4;
constexpr int kTagsPerInput = 6;

// Code point ranges a tag may hold as they are.
constexpr std::uint32_t kFirstPrintableAscii = 0x20;
constexpr std::uint32_t kLastPrintableAscii = 0x7E;
constexpr std::uint32_t kFirstGreek = 0x370;
constexpr std::uint32_t kLastGreek = 0x3FF;
constexpr std::uint32_t kFirstCyrillic = 0x400;
constexpr std::uint32_t kLastCyrillic = 0x4FF;
constexpr std::uint32_t kFirstAfterC1Controls = 0xA1;
constexpr std::uint32_t kLastBeforeSurrogates = 0xD7FF;
constexpr std::uint32_t kFirstAfterSurrogates = 0xE000;
constexpr std::uint32_t kMaxScalarValue = 0x10FFFF;
constexpr char32_t kDottedCapitalI = 0x0130;  // lowercases to two code points

// White_Space code points other than the ASCII ones: no-break space, em space, ideographic
// space, and next line.
constexpr char32_t kNoBreakSpace = 0x00A0;
constexpr char32_t kEmSpace = 0x2003;
constexpr char32_t kIdeographicSpace = 0x3000;
constexpr char32_t kNextLine = 0x0085;

rc::Gen<char32_t> code_point_in(std::uint32_t first, std::uint32_t last) {
    return rc::gen::map(rc::gen::inRange<std::uint32_t>(first, last + 1),
                        [](std::uint32_t v) { return static_cast<char32_t>(v); });
}

// A code point a tag may hold as it is: not a comma, not white space, not a control character.
// Upper-case ASCII, Greek (the sigma included), the dotted capital I, Cyrillic, and any scalar
// value are all there.
rc::Gen<char32_t> tag_code_point() {
    const auto any = rc::gen::oneOf(code_point_in(kFirstPrintableAscii, kLastPrintableAscii),
                                    code_point_in(kFirstGreek, kLastGreek),
                                    code_point_in(kFirstCyrillic, kLastCyrillic),
                                    code_point_in(kFirstAfterC1Controls, kLastBeforeSurrogates),
                                    code_point_in(kFirstAfterSurrogates, kMaxScalarValue));
    return rc::gen::suchThat(rc::gen::oneOf(any, rc::gen::just(kDottedCapitalI)), [](char32_t cp) {
        return cp != U',' && !is_white_space(cp) && !is_control(cp);
    });
}

// White space between the words of a tag, and around it: a run of spaces, tabs, newlines, and
// other White_Space code points.
rc::Gen<std::string> white_space() {
    const auto one = rc::gen::element(U' ', U'\t', U'\n', U'\r', kNoBreakSpace, kEmSpace,
                                      kIdeographicSpace, kNextLine);
    return rc::gen::map(rc::gen::container<std::vector<char32_t>>(one), [](const auto& list) {
        std::string out;
        for (const char32_t cp : list) {
            append_utf8(out, cp);
        }
        return out;
    });
}

rc::Gen<std::string> word() {
    return rc::gen::map(
        rc::gen::resize(kWordSize, rc::gen::nonEmpty(rc::gen::container<std::vector<char32_t>>(
                                       tag_code_point()))),
        [](const auto& list) {
            std::string out;
            for (const char32_t cp : list) {
                append_utf8(out, cp);
            }
            return out;
        });
}

// One tag as typed: words with white space between them and around them. It may be empty.
rc::Gen<std::string> typed_tag() {
    return rc::gen::map(
        rc::gen::pair(
            white_space(),
            rc::gen::resize(kWordsPerTag,
                            rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
                                rc::gen::pair(word(), white_space())))),
        [](const auto& parts) {
            std::string out = parts.first;
            for (const auto& [text, space] : parts.second) {
                out += text + space;
            }
            return out;
        });
}

// Tag input: typed tags joined by commas, which may include empty tags.
rc::Gen<std::string> tag_input() {
    return rc::gen::map(
        rc::gen::resize(kTagsPerInput, rc::gen::container<std::vector<std::string>>(typed_tag())),
        [](const auto& tags) {
            std::string out;
            for (std::size_t i = 0; i < tags.size(); ++i) {
                out += (i == 0 ? "" : ",") + tags[i];
            }
            return out;
        });
}

TagSet parsed(const std::string& input) {
    const auto result = parse_tags(input);
    RC_ASSERT(result.has_value());
    return *result;
}

std::string swap_ascii_case(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        } else if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

}  // namespace

// Product section 4 Tags.
TEST_CASE("property: parsing tags is idempotent and adds no duplicate (product 4 Tags)",
          "[core][tags][property]") {
    rc::prop("parsing the parsed tags gives the same set", [] {
        const TagSet tags = parsed(*tag_input());
        std::string joined;
        for (const std::string& tag : tags) {
            joined += (joined.empty() ? "" : ",") + tag;
        }
        RC_ASSERT(parsed(joined) == tags);
    });
    rc::prop("the same input twice gives the same set", [] {
        const std::string input = *tag_input();
        RC_ASSERT(parsed(input + "," + input) == parsed(input));
    });
}

TEST_CASE("property: every parsed tag follows the tag rules (product 4 Tags)",
          "[core][tags][property]") {
    rc::prop(
        "non-empty, lowercased, trimmed, one space for a whitespace run, no control, at most "
        "128 code points",
        [] {
            for (const std::string& tag : parsed(*tag_input())) {
                RC_ASSERT(!tag.empty());
                RC_ASSERT(to_lower(tag) == tag);
                const auto code_points = decode_utf8(tag);
                RC_ASSERT(code_points.has_value());
                RC_ASSERT(code_points->size() <= kTagLimit);
                RC_ASSERT(!is_white_space(code_points->front()));
                RC_ASSERT(!is_white_space(code_points->back()));
                for (const char32_t cp : *code_points) {
                    RC_ASSERT(!is_control(cp));
                    RC_ASSERT(cp == U' ' || !is_white_space(cp));
                }
                RC_ASSERT(tag.find("  ") == std::string::npos);
            }
        });
}

TEST_CASE("property: changing the ASCII case of the input changes no tag (product 4 Tags)",
          "[core][tags][property]") {
    rc::prop("the case-swapped input gives the same set", [] {
        const std::string input = *tag_input();
        RC_ASSERT(parsed(swap_ascii_case(input)) == parsed(input));
    });
}

namespace {

constexpr std::size_t kNearLimitSpan = 8;
constexpr std::size_t kNearLimitFirst = kTagLimit - kNearLimitSpan;
constexpr std::size_t kNearLimitLast = kTagLimit + kNearLimitSpan;

}  // namespace

TEST_CASE("property: a tag has at most 128 code points after lowercasing (product 4 Tags)",
          "[core][tags][property]") {
    rc::prop("a tag near the limit is accepted exactly when its lowercase has at most 128", [] {
        const auto length = *rc::gen::inRange<std::size_t>(kNearLimitFirst, kNearLimitLast + 1);
        // Mostly 'a' (one code point), sometimes U+0130, which lowercases to two.
        const auto code_points = *rc::gen::container<std::vector<char32_t>>(
            length, rc::gen::weightedElement<char32_t>({{3, U'a'}, {1, kDottedCapitalI}}));
        std::string tag;
        for (const char32_t cp : code_points) {
            append_utf8(tag, cp);
        }
        const auto lowered = decode_utf8(to_lower(tag));
        RC_ASSERT(lowered.has_value());
        RC_ASSERT(parse_tags(tag).has_value() == (lowered->size() <= kTagLimit));
    });
}
