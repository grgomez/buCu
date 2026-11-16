#pragma once

// Helpers the core property tests share: encoding a list of code points, and drawing a code
// point from a range.

#include <rapidcheck.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/utf8.hpp"

namespace bucu::core::test {

// Code point boundaries the property tests draw from.
inline constexpr std::uint32_t kFirstPrintableAscii = 0x20;
inline constexpr std::uint32_t kLastPrintableAscii = 0x7E;
inline constexpr std::uint32_t kFirstAfterC1Controls = 0xA1;
inline constexpr std::uint32_t kLastLatinExtendedB = 0x24F;
inline constexpr std::uint32_t kFirstGreek = 0x370;
inline constexpr std::uint32_t kLastGreek = 0x3FF;
inline constexpr std::uint32_t kFirstCyrillic = 0x400;
inline constexpr std::uint32_t kLastCyrillic = 0x4FF;
inline constexpr std::uint32_t kFirstSurrogate = 0xD800;
inline constexpr std::uint32_t kLastSurrogate = 0xDFFF;
inline constexpr std::uint32_t kMaxScalarValue = 0x10FFFF;
inline constexpr char32_t kDottedCapitalI = 0x0130;  // lowercases to two code points

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
