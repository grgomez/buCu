#pragma once

// Helpers the core property tests share: encoding a list of code points, and drawing a code
// point from a range.

#include <rapidcheck.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/utf8.hpp"

namespace bucu::core::test {

// The UTF-8 text of `code_points`, in order.
inline std::string encode(const std::vector<char32_t>& code_points) {
    std::string out;
    for (const char32_t cp : code_points) {
        append_utf8(out, cp);
    }
    return out;
}

// A code point from `first` to `last`, both included.
inline rc::Gen<char32_t> code_point_in(std::uint32_t first, std::uint32_t last) {
    return rc::gen::map(rc::gen::inRange<std::uint32_t>(first, last + 1),
                        [](std::uint32_t v) { return static_cast<char32_t>(v); });
}

}  // namespace bucu::core::test
