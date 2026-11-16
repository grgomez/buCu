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

// Code point boundaries of the UTF-8 length classes (RFC 3629).
constexpr std::uint32_t kLastOneByte = 0x7F;
constexpr std::uint32_t kFirstTwoByte = 0x80;
constexpr std::uint32_t kLastTwoByte = 0x7FF;
constexpr std::uint32_t kFirstThreeByte = 0x800;
constexpr std::uint32_t kLastThreeByte = 0xFFFF;
constexpr std::uint32_t kFirstFourByte = 0x10000;
constexpr std::uint32_t kMaxScalarValue = 0x10FFFF;
constexpr std::uint32_t kFirstSurrogate = 0xD800;
constexpr std::uint32_t kLastSurrogate = 0xDFFF;
constexpr std::uint32_t kAfterSurrogates = kLastSurrogate + 1;

// Bytes that make a sequence malformed (RFC 3629).
constexpr unsigned kFirstContinuation = 0x80;
constexpr unsigned kLastContinuation = 0xBF;
constexpr unsigned kFirstOverlongTwoLead = 0xC0;  // C0 and C1 can only start an overlong form
constexpr unsigned kLastOverlongTwoLead = 0xC1;
constexpr unsigned kThreeByteLead = 0xE0;
constexpr unsigned kLastOverlongThreeSecond = 0x9F;  // E0 80 to E0 9F is overlong
constexpr unsigned kFourByteLead = 0xF0;
constexpr unsigned kLastOverlongFourSecond = 0x8F;  // F0 80 to F0 8F is overlong
constexpr unsigned kSurrogateLead = 0xED;
constexpr unsigned kFirstSurrogateSecond = 0xA0;  // ED A0 to ED BF encodes a surrogate
constexpr unsigned kLastScalarLead = 0xF4;
constexpr unsigned kFirstTooBigSecond = 0x90;  // F4 90 and up is above U+10FFFF
constexpr unsigned kFirstTooBigLead = 0xF5;
constexpr unsigned kLastTooBigLead = 0xF7;
constexpr unsigned kFirstInvalidByte = 0xF8;  // F8 to FF never occur in UTF-8
constexpr unsigned kLastByte = 0xFF;

// How many bytes a truncated sequence loses: at least one, at most three.
constexpr std::size_t kLeastBytesCut = 1;
constexpr std::size_t kMostBytesCut = 3;

rc::Gen<char32_t> code_point_in(std::uint32_t first, std::uint32_t last) {
    return rc::gen::map(rc::gen::inRange<std::uint32_t>(first, last + 1),
                        [](std::uint32_t v) { return static_cast<char32_t>(v); });
}

char byte_value(unsigned value) { return static_cast<char>(value); }

// A Unicode scalar value: every code point except the surrogates. The five ranges give each
// encoded length (one to four bytes) the same weight.
rc::Gen<char32_t> scalar_value() {
    return rc::gen::oneOf(code_point_in(0, kLastOneByte),
                          code_point_in(kFirstTwoByte, kLastTwoByte),
                          code_point_in(kFirstThreeByte, kFirstSurrogate - 1),
                          code_point_in(kAfterSurrogates, kLastThreeByte),
                          code_point_in(kFirstFourByte, kMaxScalarValue));
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

std::string bytes(std::initializer_list<char> list) { return {list}; }

// A fragment that is never well-formed on its own and cannot be completed by the text after it,
// because that text starts at a code point boundary (product section 4).
rc::Gen<std::string> malformed_fragment() {
    const auto cont = byte_in(kFirstContinuation, kLastContinuation);
    const auto lone_continuation = rc::gen::map(cont, [](char c) { return bytes({c}); });
    const auto overlong2 =
        rc::gen::map(rc::gen::pair(byte_in(kFirstOverlongTwoLead, kLastOverlongTwoLead), cont),
                     [](const auto& p) { return bytes({p.first, p.second}); });
    const auto overlong3 = rc::gen::map(
        rc::gen::pair(byte_in(kFirstContinuation, kLastOverlongThreeSecond), cont),
        [](const auto& p) { return bytes({byte_value(kThreeByteLead), p.first, p.second}); });
    const auto overlong4 = rc::gen::map(
        rc::gen::tuple(byte_in(kFirstContinuation, kLastOverlongFourSecond), cont, cont),
        [](const auto& t) {
            return bytes(
                {byte_value(kFourByteLead), std::get<0>(t), std::get<1>(t), std::get<2>(t)});
        });
    const auto surrogate = rc::gen::map(
        rc::gen::pair(byte_in(kFirstSurrogateSecond, kLastContinuation), cont),
        [](const auto& p) { return bytes({byte_value(kSurrogateLead), p.first, p.second}); });
    const auto too_big_f4 =
        rc::gen::map(rc::gen::tuple(byte_in(kFirstTooBigSecond, kLastContinuation), cont, cont),
                     [](const auto& t) {
                         return bytes({byte_value(kLastScalarLead), std::get<0>(t), std::get<1>(t),
                                       std::get<2>(t)});
                     });
    const auto too_big_lead = rc::gen::map(
        rc::gen::tuple(byte_in(kFirstTooBigLead, kLastTooBigLead), cont, cont, cont),
        [](const auto& t) {
            return bytes({std::get<0>(t), std::get<1>(t), std::get<2>(t), std::get<3>(t)});
        });
    const auto invalid_byte =
        rc::gen::map(byte_in(kFirstInvalidByte, kLastByte), [](char c) { return bytes({c}); });
    // A multi-byte encoding with at least one byte cut off the end.
    const auto truncated = rc::gen::map(
        rc::gen::pair(rc::gen::inRange<char32_t>(kFirstTwoByte, kMaxScalarValue + 1),
                      rc::gen::inRange<std::size_t>(kLeastBytesCut, kMostBytesCut + 1)),
        [](const auto& p) {
            std::string encoded;
            if (p.first >= kFirstSurrogate && p.first <= kLastSurrogate) {
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

// Product section 4: "All text MUST be valid UTF-8".
TEST_CASE("property: every scalar value round-trips through UTF-8 (product 4)",
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

TEST_CASE("property: malformed UTF-8 is refused by both functions (product 4)",
          "[core][utf8][property]") {
    rc::prop("a malformed fragment inserted into valid text is not valid and does not decode", [] {
        const std::string text = *valid_text() + *malformed_fragment() + *valid_text();
        RC_ASSERT(!is_valid_utf8(text));
        RC_ASSERT(!decode_utf8(text).has_value());
    });
}

TEST_CASE("property: is_valid_utf8 and decode_utf8 agree on any bytes (product 4)",
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
