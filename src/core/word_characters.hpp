#pragma once

namespace bucu::core {

// A word character of whole-word search (product section 5 search): Unicode general category L
// or N (Unicode 18.0.0 tables in third_party/unicode/), or `_`. This is Python's `\w`.
bool is_word_character(char32_t code_point);

}  // namespace bucu::core
