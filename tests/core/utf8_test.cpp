#include "core/utf8.hpp"

#include <string>
#include <string_view>

#include "catch_amalgamated.hpp"

namespace {

using bucu::core::decode_utf8;
using bucu::core::is_valid_utf8;

}  // namespace

TEST_CASE("UTF-8 validation accepts well-formed text", "[core][utf8]") {
    CHECK(is_valid_utf8(""));
    CHECK(is_valid_utf8("plain ASCII\n"));
    CHECK(is_valid_utf8("\xC3\xB1"));          // U+00F1, two bytes
    CHECK(is_valid_utf8("\xE2\x82\xAC"));      // U+20AC, three bytes
    CHECK(is_valid_utf8("\xF0\x9F\x98\x80"));  // U+1F600, four bytes
    CHECK(is_valid_utf8("\xF4\x8F\xBF\xBF"));  // U+10FFFF, the last code point
    CHECK(is_valid_utf8(std::string_view("a\0b", 3)));
}

TEST_CASE("UTF-8 decoding returns code points", "[core][utf8]") {
    auto decoded = decode_utf8("a\xC3\xB1\xE2\x82\xAC\xF0\x9F\x98\x80");
    REQUIRE(decoded.has_value());
    CHECK(decoded == std::u32string{U'a', 0xF1, 0x20AC, 0x1F600});
    const auto empty = decode_utf8("");
    REQUIRE(empty.has_value());
    CHECK(empty == std::u32string{});
    CHECK_FALSE(decode_utf8("\xC3").has_value());
}
