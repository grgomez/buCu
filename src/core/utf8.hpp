#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace bucu::core {

// True when `text` is well-formed UTF-8 (RFC 3629): no overlong forms, no surrogates, nothing
// above U+10FFFF.
bool is_valid_utf8(std::string_view text);

// The code points of `text`, or nothing when it is not well-formed UTF-8.
std::optional<std::u32string> decode_utf8(std::string_view text);

// Append the UTF-8 encoding of `code_point`, which must be a Unicode scalar value.
void append_utf8(std::string& out, char32_t code_point);

}  // namespace bucu::core
