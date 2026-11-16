#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <tuple>
#include <vector>

#include "catch_amalgamated.hpp"
// rapidcheck/catch.h needs Catch2's header first.
#include <rapidcheck/catch.h>

#include "core/utf8.hpp"

namespace {

using bucu::core::append_utf8;
using bucu::core::decode_utf8;
using bucu::core::is_valid_utf8;

// A Unicode scalar value: every code point except the surrogates. The four ranges give each
// encoded length (one to four bytes) the same weight.
rc::Gen<char32_t> scalar_value() {
    const auto in = [](std::uint32_t first, std::uint32_t last) {
        return rc::gen::map(rc::gen::inRange<std::uint32_t>(first, last + 1),
                            [](std::uint32_t v) { return static_cast<char32_t>(v); });
    };
    return rc::gen::oneOf(in(0x0, 0x7F), in(0x80, 0x7FF), in(0x800, 0xD7FF) /* below surrogates */,
                          in(0xE000, 0xFFFF), in(0x10000, 0x10FFFF));
}

std::string encode(const std::vector<char32_t>& code_points) {
    std::string out;
    for (const char32_t cp : code_points) {
        append_utf8(out, cp);
    }
    return out;
}

// Well-formed text of any length, including empty.
rc::Gen<std::string> valid_text() {
    return rc::gen::map(rc::gen::container<std::vector<char32_t>>(scalar_value()), encode);
}

rc::Gen<char> byte_in(unsigned first, unsigned last) {
    return rc::gen::map(rc::gen::inRange<unsigned>(first, last + 1),
                        [](unsigned v) { return static_cast<char>(v); });
}

std::string bytes(std::initializer_list<char> list) { return std::string(list); }

// A fragment that is never well-formed on its own and cannot be completed by the text after it,
// because that text starts at a code point boundary (product section 4 Text).
rc::Gen<std::string> malformed_fragment() {
    const auto cont = byte_in(0x80, 0xBF);
    const auto lone_continuation = rc::gen::map(cont, [](char c) { return bytes({c}); });
    const auto overlong2 = rc::gen::map(rc::gen::pair(byte_in(0xC0, 0xC1), cont),
                                        [](const auto& p) { return bytes({p.first, p.second}); });
    const auto overlong3 = rc::gen::map(
        rc::gen::pair(byte_in(0x80, 0x9F), cont),
        [](const auto& p) { return bytes({static_cast<char>(0xE0), p.first, p.second}); });
    const auto overlong4 =
        rc::gen::map(rc::gen::tuple(byte_in(0x80, 0x8F), cont, cont), [](const auto& t) {
            return bytes({static_cast<char>(0xF0), std::get<0>(t), std::get<1>(t), std::get<2>(t)});
        });
    const auto surrogate = rc::gen::map(
        rc::gen::pair(byte_in(0xA0, 0xBF), cont),
        [](const auto& p) { return bytes({static_cast<char>(0xED), p.first, p.second}); });
    const auto too_big_f4 =
        rc::gen::map(rc::gen::tuple(byte_in(0x90, 0xBF), cont, cont), [](const auto& t) {
            return bytes({static_cast<char>(0xF4), std::get<0>(t), std::get<1>(t), std::get<2>(t)});
        });
    const auto too_big_lead =
        rc::gen::map(rc::gen::tuple(byte_in(0xF5, 0xF7), cont, cont, cont), [](const auto& t) {
            return bytes({std::get<0>(t), std::get<1>(t), std::get<2>(t), std::get<3>(t)});
        });
    const auto invalid_byte = rc::gen::map(byte_in(0xF8, 0xFF), [](char c) { return bytes({c}); });
    // A multi-byte encoding with at least one byte cut off the end.
    const auto truncated =
        rc::gen::map(rc::gen::pair(rc::gen::inRange<char32_t>(0x80, 0x10000),
                                   rc::gen::inRange<std::size_t>(1, 3)),
                     [](const auto& p) {
                         std::string encoded;
                         if (p.first >= 0xD800 && p.first <= 0xDFFF) {
                             append_utf8(encoded, U'\u00E9');
                         } else {
                             append_utf8(encoded, p.first);
                         }
                         encoded.resize(encoded.size() - std::min(p.second, encoded.size() - 1));
                         return encoded;
                     });
    return rc::gen::oneOf(lone_continuation, overlong2, overlong3, overlong4, surrogate, too_big_f4,
                          too_big_lead, invalid_byte, truncated);
}

}  // namespace

// Product section 4 Text: "All text MUST be valid UTF-8".
TEST_CASE("property: every scalar value round-trips through UTF-8 (product 4 Text)",
          "[core][utf8][property]") {
    rc::prop("decode of append gives the code point back, and the encoding is valid", [] {
        const char32_t code_point = *scalar_value();
        std::string encoded;
        append_utf8(encoded, code_point);
        RC_ASSERT(is_valid_utf8(encoded));
        const auto decoded = decode_utf8(encoded);
        RC_ASSERT(decoded.has_value());
        RC_ASSERT(*decoded == std::u32string(1, code_point));
    });
}

TEST_CASE("property: malformed UTF-8 is refused by both functions (product 4 Text)",
          "[core][utf8][property]") {
    rc::prop("a malformed fragment inserted into valid text is not valid and does not decode", [] {
        const std::string text = *valid_text() + *malformed_fragment() + *valid_text();
        RC_ASSERT(!is_valid_utf8(text));
        RC_ASSERT(!decode_utf8(text).has_value());
    });
}

TEST_CASE("property: is_valid_utf8 and decode_utf8 agree on any bytes (product 4 Text)",
          "[core][utf8][property]") {
    rc::prop("valid exactly when decoded, and the decoding encodes back to the bytes", [] {
        const std::string text =
            *rc::gen::oneOf(rc::gen::arbitrary<std::string>(),
                            rc::gen::map(rc::gen::pair(valid_text(), malformed_fragment()),
                                         [](const auto& p) { return p.first + p.second; }),
                            valid_text());
        const auto decoded = decode_utf8(text);
        RC_ASSERT(is_valid_utf8(text) == decoded.has_value());
        if (decoded) {
            std::string again;
            for (const char32_t cp : *decoded) {
                append_utf8(again, cp);
            }
            RC_ASSERT(again == text);
        }
    });
}
