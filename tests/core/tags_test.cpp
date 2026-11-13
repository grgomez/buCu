#include "bucu/core/tags.hpp"

#include <string>
#include <string_view>
#include <vector>

#include "catch_amalgamated.hpp"
#include "core/utf8.hpp"

namespace {

using bucu::core::parse_tags;
using bucu::core::TagSet;

std::string utf8(std::u32string_view code_points) {
    std::string out;
    for (const char32_t cp : code_points) {
        bucu::core::append_utf8(out, cp);
    }
    return out;
}

TagSet parsed(std::string_view input) {
    const auto result = parse_tags(input);
    REQUIRE(result.has_value());
    return *result;
}

std::vector<std::string> listed(std::string_view input) {
    const TagSet tags = parsed(input);
    return {tags.begin(), tags.end()};
}

using Tags = std::vector<std::string>;

}  // namespace

TEST_CASE("Tag parsing splits, trims, collapses, lowercases, and deduplicates", "[core][tags]") {
    CHECK(listed(" Linux ,cli   tools,,LINUX") == Tags{"cli tools", "linux"});
    CHECK(listed("a,b,c") == Tags{"a", "b", "c"});
    CHECK(listed("single") == Tags{"single"});
}

TEST_CASE("Tag input of only commas and whitespace is an empty set", "[core][tags]") {
    CHECK(parsed("").empty());
    CHECK(parsed(" , ,\t,\n").empty());
    CHECK(parsed(",,,").empty());
    CHECK(parsed(utf8(U"　, ")).empty());
}

TEST_CASE("Tag parsing turns any whitespace run inside a tag into one space", "[core][tags]") {
    CHECK(listed("a\tb") == Tags{"a b"});
    CHECK(listed("a\nb") == Tags{"a b"});
    CHECK(listed("a \t\r\n b") == Tags{"a b"});
    CHECK(listed(utf8(U"a　 b")) == Tags{"a b"});
    CHECK(listed("\tx\ny\t") == Tags{"x y"});
}

TEST_CASE("Tag parsing lowercases with the Unicode mapping", "[core][tags]") {
    CHECK(parsed(utf8(U"Ñandú")) == TagSet{utf8(U"ñandú")});
    CHECK(parsed(utf8(U"Ñandú,ñandú")) == TagSet{utf8(U"ñandú")});
    CHECK(parsed(utf8(U"ΣΟΦΙΑ,σοφια")) == TagSet{utf8(U"σοφια")});
    CHECK(parsed(utf8(U"İ")) == TagSet{utf8(U"i̇")});
    CHECK(parsed(utf8(U"ΣΑΣ")) == TagSet{utf8(U"σας")});
}

TEST_CASE("Tag parsing applies no Unicode normalization", "[core][tags]") {
    // U+00E9 and U+0065 U+0301 are different tags.
    CHECK(parsed(utf8(U"é,é")).size() == 2);
}

TEST_CASE("Tags are in ascending byte order", "[core][tags]") {
    CHECK(listed("b,a,B,A") == Tags{"a", "b"});
    CHECK(listed(utf8(U"é,z,Z,1")) == Tags{"1", "z", utf8(U"é")});
    CHECK(listed("a b,a,ab") == Tags{"a", "a b", "ab"});
}
