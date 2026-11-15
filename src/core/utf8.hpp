#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace bucu::core {

// A UTF-8 continuation byte is 10xxxxxx (RFC 3629).
inline constexpr unsigned kContinuationMask = 0xC0;
inline constexpr unsigned kContinuationTag = 0x80;

// True when `byte` is a continuation byte, not the first byte of a code point.
constexpr bool is_utf8_continuation(unsigned char byte) {
    return (byte & kContinuationMask) == kContinuationTag;
}

// True when `text` is well-formed UTF-8 (RFC 3629): no overlong forms, no surrogates, nothing
// above U+10FFFF.
bool is_valid_utf8(std::string_view text);

// The code points of `text`, or nothing when it is not well-formed UTF-8.
std::optional<std::u32string> decode_utf8(std::string_view text);

// Append the UTF-8 encoding of `code_point`, which must be a Unicode scalar value.
void append_utf8(std::string& out, char32_t code_point);

}  // namespace bucu::core
