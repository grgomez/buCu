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

namespace {

using bucu::core::Field;
using bucu::core::Reason;

void check_tag_refused(std::string_view input, Reason reason, std::string_view value) {
    INFO("input: " << input);
    const auto result = parse_tags(input);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kTag);
    CHECK(result.error().reason == reason);
    CHECK(result.error().value == value);
}

std::string repeat(std::u32string_view unit, int times) {
    std::string out;
    for (int i = 0; i < times; ++i) {
        out += utf8(unit);
    }
    return out;
}

}  // namespace

TEST_CASE("Tag parsing refuses a control character and names the tag", "[core][tags]") {
    check_tag_refused(
        "ok,a\x01"
        "b",
        Reason::kControlCharacter,
        "a\x01"
        "b");
    check_tag_refused("a\x7F", Reason::kControlCharacter, "a\x7F");
    check_tag_refused(utf8(U"a\u0090"), Reason::kControlCharacter, utf8(U"a\u0090"));
    check_tag_refused(std::string("a\0b", 3), Reason::kControlCharacter, std::string("a\0b", 3));
}

TEST_CASE("Tag parsing refuses invalid UTF-8 and names the tag", "[core][tags]") {
    check_tag_refused("ok,bad\xC3", Reason::kInvalidUtf8, "bad\xC3");
    check_tag_refused("\xFF", Reason::kInvalidUtf8, "\xFF");
    check_tag_refused("a\xC0\xAF,b", Reason::kInvalidUtf8, "a\xC0\xAF");
}

TEST_CASE("A tag may have 128 code points and no more", "[core][tags]") {
    const std::string at_limit(128, 'a');
    CHECK(parsed(at_limit) == TagSet{at_limit});
    const std::string too_long(129, 'a');
    check_tag_refused(too_long, Reason::kTooLong, too_long);
    // The limit counts code points, not bytes.
    CHECK(parsed(repeat(U"ñ", 128)).size() == 1);
    check_tag_refused(repeat(U"ñ", 129), Reason::kTooLong, repeat(U"ñ", 129));
}

TEST_CASE("The tag limit counts code points after lowercasing", "[core][tags]") {
    // U+0130 lowercases to two code points: 64 of them make 128, 65 make 130.
    CHECK(parsed(repeat(U"İ", 64)).size() == 1);
    check_tag_refused(repeat(U"İ", 65), Reason::kTooLong, repeat(U"i̇", 65));
    // U+023A grows from two bytes to three but stays one code point.
    CHECK(parsed(repeat(U"Ⱥ", 128)).size() == 1);
    check_tag_refused(repeat(U"Ⱥ", 129), Reason::kTooLong, repeat(U"ⱥ", 129));
}

TEST_CASE("Spaces inside a tag count toward the limit after collapsing", "[core][tags]") {
    const std::string spaced = std::string(64, 'a') + "     " + std::string(63, 'b');
    CHECK(parsed(spaced).size() == 1);  // 64 + 1 + 63 = 128
    const std::string over = std::string(64, 'a') + "     " + std::string(64, 'b');
    check_tag_refused(over, Reason::kTooLong, std::string(64, 'a') + " " + std::string(64, 'b'));
}

TEST_CASE("Adding tags is a union", "[core][tags]") {
    using bucu::core::add_tags;
    CHECK(add_tags(parsed("a,b"), parsed("b,c")) == parsed("a,b,c"));
    CHECK(add_tags(parsed("a"), parsed("")) == parsed("a"));
    CHECK(add_tags(parsed(""), parsed("x")) == parsed("x"));
    CHECK(add_tags(parsed(""), parsed("")).empty());
}

TEST_CASE("Removing tags is a difference and an absent tag is not an error", "[core][tags]") {
    using bucu::core::remove_tags;
    CHECK(remove_tags(parsed("a,b,c"), parsed("a,z")) == parsed("b,c"));
    CHECK(remove_tags(parsed("a,b"), parsed("a,b")).empty());
    CHECK(remove_tags(parsed("a"), parsed("z")) == parsed("a"));
    CHECK(remove_tags(parsed(""), parsed("a")).empty());
}

TEST_CASE("Replacing tags is assigning a parsed set, which may be empty", "[core][tags]") {
    TagSet tags = parsed("a,b");
    tags = parsed("x");
    CHECK(tags == TagSet{"x"});
    tags = parsed("");
    CHECK(tags.empty());
}

TEST_CASE("Tag operations keep the set sorted", "[core][tags]") {
    using bucu::core::add_tags;
    const TagSet result = add_tags(parsed("b"), parsed("c,a"));
    CHECK(Tags(result.begin(), result.end()) == Tags{"a", "b", "c"});
}
