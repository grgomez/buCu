#include "bucu/core/bookmark.hpp"

#include <cstdint>
#include <string>

#include "catch_amalgamated.hpp"

using bucu::core::Bookmark;
using bucu::core::clear_flag;
using bucu::core::has_flag;
using bucu::core::kFlagImmutableTitle;
using bucu::core::set_flag;

TEST_CASE("A new bookmark has no flag set and no tags", "[core][bookmark]") {
    const Bookmark bookmark;
    CHECK(bookmark.flags == 0);
    CHECK_FALSE(has_flag(bookmark.flags, kFlagImmutableTitle));
    CHECK(bookmark.tags.empty());
    CHECK(bookmark.url.empty());
}

TEST_CASE("The immutable-title flag is Buku's bit 1", "[core][bookmark]") {
    CHECK(kFlagImmutableTitle == 1);
}

TEST_CASE("Setting and clearing the flag gives stored values 1 and 0", "[core][bookmark]") {
    std::int64_t flags = 0;
    flags = set_flag(flags, kFlagImmutableTitle);
    CHECK(flags == 1);
    CHECK(has_flag(flags, kFlagImmutableTitle));
    flags = clear_flag(flags, kFlagImmutableTitle);
    CHECK(flags == 0);
    CHECK_FALSE(has_flag(flags, kFlagImmutableTitle));
}

TEST_CASE("Setting or clearing a set or clear flag changes nothing", "[core][bookmark]") {
    CHECK(set_flag(1, kFlagImmutableTitle) == 1);
    CHECK(clear_flag(0, kFlagImmutableTitle) == 0);
}

TEST_CASE("Changing the flag keeps the bits BuCu does not know", "[core][bookmark]") {
    CHECK(set_flag(6, kFlagImmutableTitle) == 7);
    CHECK(clear_flag(7, kFlagImmutableTitle) == 6);
    CHECK(clear_flag(5, kFlagImmutableTitle) == 4);
    CHECK(has_flag(6, kFlagImmutableTitle) == false);
    CHECK(has_flag(7, kFlagImmutableTitle));
    constexpr std::int64_t kHighBit = INT64_MIN;
    CHECK(set_flag(kHighBit, kFlagImmutableTitle) == (kHighBit | 1));
    CHECK(clear_flag(kHighBit | 1, kFlagImmutableTitle) == kHighBit);
}

namespace {

using bucu::core::Field;
using bucu::core::make_bookmark;
using bucu::core::Reason;
using bucu::core::TagSet;

}  // namespace

TEST_CASE("make_bookmark normalizes the URL and keeps the text as given", "[core][bookmark]") {
    const auto result = make_bookmark("HTTPS://Example.ORG:443", " A Title\n", "line 1\nline 2\n",
                                      TagSet{"a", "b"});
    REQUIRE(result.has_value());
    CHECK(result->url == "https://example.org/");
    CHECK(result->title == " A Title\n");
    CHECK(result->description == "line 1\nline 2\n");
    CHECK(result->tags == TagSet{"a", "b"});
    CHECK(result->flags == 0);
}

TEST_CASE("make_bookmark accepts an empty title and description", "[core][bookmark]") {
    const auto result = make_bookmark("about:blank", "", "");
    REQUIRE(result.has_value());
    CHECK(result->title.empty());
    CHECK(result->description.empty());
    CHECK(result->tags.empty());
}

TEST_CASE("make_bookmark accepts any valid UTF-8 text without a length limit", "[core][bookmark]") {
    const std::string long_text(100000, 'x');
    const std::string text = "\xC3\xB1\xE2\x82\xAC\xF0\x9F\x98\x80\t\x01";
    const auto result = make_bookmark("about:blank", text, long_text);
    REQUIRE(result.has_value());
    CHECK(result->title == text);
    CHECK(result->description == long_text);
}

TEST_CASE("make_bookmark refuses invalid UTF-8 and names the field", "[core][bookmark]") {
    const auto title = make_bookmark("about:blank", "bad\xC3", "ok");
    REQUIRE_FALSE(title.has_value());
    CHECK(title.error().field == Field::kTitle);
    CHECK(title.error().reason == Reason::kInvalidUtf8);
    CHECK(title.error().value == "bad\xC3");

    const auto description = make_bookmark("about:blank", "ok", "\xFF");
    REQUIRE_FALSE(description.has_value());
    CHECK(description.error().field == Field::kDescription);
    CHECK(description.error().reason == Reason::kInvalidUtf8);
}

TEST_CASE("make_bookmark refuses an invalid URL and names the field", "[core][bookmark]") {
    const auto result = make_bookmark("example.org", "t", "d");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kUrl);
    CHECK(result.error().reason == Reason::kNoScheme);
}

TEST_CASE("make_bookmark reports the URL before the title and description", "[core][bookmark]") {
    const auto result = make_bookmark("", "\xFF", "\xFF");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kUrl);
}
