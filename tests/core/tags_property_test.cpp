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
constexpr std::size_t kLimit = 128;

rc::Gen<char32_t> code_point_in(std::uint32_t first, std::uint32_t last) {
    return rc::gen::map(rc::gen::inRange<std::uint32_t>(first, last + 1),
                        [](std::uint32_t v) { return static_cast<char32_t>(v); });
}

// A code point a tag may hold as it is: not a comma, not white space, not a control character.
// Upper-case ASCII, Greek (the sigma included), the dotted capital I, Cyrillic, and any scalar
// value are all there.
rc::Gen<char32_t> tag_code_point() {
    const auto any = rc::gen::oneOf(code_point_in(0x20, 0x7E), code_point_in(0x370, 0x3FF),
                                    code_point_in(0x400, 0x4FF), code_point_in(0xA1, 0xD7FF),
                                    code_point_in(0xE000, 0x10FFFF));
    return rc::gen::suchThat(rc::gen::oneOf(any, rc::gen::just(char32_t{0x0130})), [](char32_t cp) {
        return cp != U',' && !is_white_space(cp) && !is_control(cp);
    });
}

// White space between the words of a tag, and around it: a run of spaces, tabs, newlines, and
// other White_Space code points.
rc::Gen<std::string> white_space() {
    const auto one =
        rc::gen::element(char32_t{U' '}, char32_t{U'\t'}, char32_t{U'\n'}, char32_t{U'\r'},
                         char32_t{0x00A0}, char32_t{0x2003}, char32_t{0x3000}, char32_t{0x0085});
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
        rc::gen::resize(
            8, rc::gen::nonEmpty(rc::gen::container<std::vector<char32_t>>(tag_code_point()))),
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
            rc::gen::resize(4, rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
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
        rc::gen::resize(6, rc::gen::container<std::vector<std::string>>(typed_tag())),
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
                RC_ASSERT(code_points->size() <= kLimit);
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
