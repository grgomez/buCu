#include "core/unicode_lower.hpp"

#include <iterator>
#include <string>
#include <string_view>

#include "catch_amalgamated.hpp"
#include "core/utf8.hpp"

namespace {

using bucu::core::is_control;
using bucu::core::is_white_space;
using bucu::core::to_lower;

// Build UTF-8 from code point escapes, so the tests do not depend on the compiler's source or
// execution character set (constitution principle 12).
std::string utf8(std::u32string_view code_points) {
    std::string out;
    for (const char32_t cp : code_points) {
        bucu::core::append_utf8(out, cp);
    }
    return out;
}

std::string lower(std::u32string_view code_points) { return to_lower(utf8(code_points)); }

}  // namespace

TEST_CASE("Lowercase maps one code point from four scripts", "[core][lower]") {
    CHECK(to_lower("ABC xyz 123") == "abc xyz 123");
    CHECK(lower(U"ÑANDÚ") == utf8(U"ñandú"));            // Latin
    CHECK(lower(U"ΑΒΓ") == utf8(U"αβγ"));                // Greek
    CHECK(lower(U"ЖИТ") == utf8(U"жит"));                // Cyrillic
    CHECK(lower(U"ԱԲ") == utf8(U"աբ"));                  // Armenian
    CHECK(lower(U"Ꭰ") == utf8(U"ꭰ"));                    // Cherokee
    CHECK(lower(U"\U0001E900") == utf8(U"\U0001E922"));  // Adlam, plane 1
    CHECK(lower(U"Ɐ") == utf8(U"ɐ"));                    // Latin Extended-C
    CHECK(lower(U"ß中あ") == utf8(U"ß中あ"));            // no mapping
}

TEST_CASE("Lowercase turns U+0130 into i and a combining dot", "[core][lower]") {
    CHECK(lower(U"İ") == utf8(U"i̇"));
    CHECK(lower(U"İİ") == utf8(U"i̇i̇"));
}

TEST_CASE("Lowercase applies Final_Sigma", "[core][lower]") {
    CHECK(lower(U"Σ") == utf8(U"σ"));      // alone
    CHECK(lower(U"ΑΣ") == utf8(U"ας"));    // end of word
    CHECK(lower(U"ΣΑΣ") == utf8(U"σας"));  // start and end
    CHECK(lower(U"ΑΣΑ") == utf8(U"ασα"));  // inside
    CHECK(lower(U"ΣΟΦΙΑ") == utf8(U"σοφια"));
    CHECK(lower(U"1Σ") == utf8(U"1σ"));    // no cased letter before
    CHECK(lower(U"Α.Σ") == utf8(U"α.ς"));  // '.' is Case_Ignorable
    CHECK(lower(U"ΑΣ.") == utf8(U"ας."));
    CHECK(lower(U"ΑΣ.Α") == utf8(U"ασ.α"));  // cased letter after
    CHECK(lower(U"ΑΣ Α") == utf8(U"ας α"));  // space is not ignorable
    CHECK(lower(U"σΣ") == utf8(U"σς"));      // lowercase sigma is Cased
}

TEST_CASE("Lowercase leaves an empty string empty", "[core][lower]") {
    CHECK(to_lower("").empty());
}

TEST_CASE("White_Space is the 25 Unicode code points", "[core][lower]") {
    constexpr char32_t kSpaces[] = {0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020, 0x0085,
                                    0x00A0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004,
                                    0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x2028,
                                    0x2029, 0x202F, 0x205F, 0x3000};
    CHECK(std::size(kSpaces) == 25);
    for (const char32_t cp : kSpaces) {
        CHECK(is_white_space(cp));
    }
    for (const char32_t cp :
         {char32_t{0x0008}, char32_t{0x000E}, char32_t{0x001F}, char32_t{'a'}, char32_t{0x200B},
          char32_t{0x180E}, char32_t{0xFEFF}, char32_t{0x3001}}) {
        CHECK_FALSE(is_white_space(cp));
    }
}

TEST_CASE("Control is general category Cc", "[core][lower]") {
    for (char32_t cp = 0; cp <= 0x1F; ++cp) {
        CHECK(is_control(cp));
    }
    for (char32_t cp = 0x7F; cp <= 0x9F; ++cp) {
        CHECK(is_control(cp));
    }
    for (const char32_t cp : {char32_t{0x20}, char32_t{'a'}, char32_t{0x7E}, char32_t{0xA0},
                              char32_t{0x200B}, char32_t{0x2028}}) {
        CHECK_FALSE(is_control(cp));
    }
}
