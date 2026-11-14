#pragma once

#include <string>
#include <string_view>

namespace bucu::core {

// The Unicode full lowercase mapping of `utf8`, without language tailoring: the per-character
// mapping, U+0130 as U+0069 U+0307, and the Final_Sigma rule (Unicode 18.0.0 tables in
// third_party/unicode/). It equals Python's str.lower(). `utf8` must be well-formed UTF-8;
// malformed text comes back unchanged.
std::string to_lower(std::string_view utf8);

// The Unicode White_Space property (Unicode 18.0.0).
bool is_white_space(char32_t code_point);

// General category Cc: U+0000 to U+001F and U+007F to U+009F.
bool is_control(char32_t code_point);

}  // namespace bucu::core
