#include <cstdint>
#include <string>

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
