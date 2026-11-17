#include "bucu/core/bookmark.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

#include "catch_amalgamated.hpp"

using bucu::core::Bookmark;
using bucu::core::clear_flag;
using bucu::core::Flag;
using bucu::core::FlagSet;
using bucu::core::has_flag;
using bucu::core::kFlagImmutableTitle;
using bucu::core::set_flag;

// A raw integer never becomes a Flag or a FlagSet by itself (US3-AC1).
static_assert(!std::is_convertible_v<std::int64_t, Flag>);
static_assert(!std::is_convertible_v<std::int64_t, FlagSet>);
static_assert(!std::is_convertible_v<int, FlagSet>);
static_assert(std::is_constructible_v<FlagSet, std::int64_t>);
static_assert(std::is_constructible_v<Flag, std::int64_t>);
// A flag set never becomes a raw integer or a single flag by itself.
static_assert(!std::is_convertible_v<FlagSet, std::int64_t>);
static_assert(!std::is_convertible_v<FlagSet, Flag>);
static_assert(!std::is_convertible_v<Flag, std::int64_t>);
// A single flag is a flag set of one flag.
static_assert(std::is_convertible_v<Flag, FlagSet>);

TEST_CASE("A new bookmark has no flag set and no tags", "[core][bookmark]") {
    const Bookmark bookmark;
    CHECK(bookmark.flags == FlagSet());
    CHECK(bookmark.flags.bits() == 0);
    CHECK_FALSE(has_flag(bookmark.flags, kFlagImmutableTitle));
    CHECK(bookmark.tags.empty());
    CHECK(bookmark.url.empty());
}

TEST_CASE("The immutable-title flag is Buku's bit 1", "[core][bookmark]") {
    CHECK(kFlagImmutableTitle.bits() == 1);
}

TEST_CASE("A flag converts to a flag set of one flag", "[core][bookmark]") {
    Bookmark bookmark;
    bookmark.flags = kFlagImmutableTitle;
    CHECK(bookmark.flags.bits() == 1);
}

TEST_CASE("Setting and clearing the flag gives stored values 1 and 0", "[core][bookmark]") {
    FlagSet flags;
    flags = set_flag(flags, kFlagImmutableTitle);
    CHECK(flags.bits() == 1);
    CHECK(has_flag(flags, kFlagImmutableTitle));
    flags = clear_flag(flags, kFlagImmutableTitle);
    CHECK(flags.bits() == 0);
    CHECK_FALSE(has_flag(flags, kFlagImmutableTitle));
}

TEST_CASE("Changing the flag keeps the bits BuCu does not know", "[core][bookmark]") {
    CHECK(set_flag(FlagSet(6), kFlagImmutableTitle).bits() == 7);
    CHECK(clear_flag(FlagSet(7), kFlagImmutableTitle).bits() == 6);
    CHECK(clear_flag(FlagSet(5), kFlagImmutableTitle).bits() == 4);
    CHECK(has_flag(FlagSet(6), kFlagImmutableTitle) == false);
    CHECK(has_flag(FlagSet(7), kFlagImmutableTitle));
    constexpr std::int64_t kHighBit = INT64_MIN;
    CHECK(set_flag(FlagSet(kHighBit), kFlagImmutableTitle).bits() == (kHighBit | 1));
    CHECK(clear_flag(FlagSet(kHighBit | 1), kFlagImmutableTitle).bits() == kHighBit);
}

namespace {

using bucu::core::Field;
using bucu::core::make_bookmark;
using bucu::core::Reason;
using bucu::core::TagSet;

}  // namespace

TEST_CASE("make_bookmark normalizes the URL and keeps the text as given", "[core][bookmark]") {
    const auto result = make_bookmark({.url = "HTTPS://Example.ORG:443",
                                       .title = " A Title\n",
                                       .description = "line 1\nline 2\n",
                                       .tags = " B, a,,A"});
    REQUIRE(result.has_value());
    CHECK(result->url == "https://example.org/");
    CHECK(result->title == " A Title\n");
    CHECK(result->description == "line 1\nline 2\n");
    CHECK(result->tags == TagSet{"a", "b"});
    CHECK(result->flags.bits() == 0);
}

TEST_CASE("make_bookmark accepts an empty title and description", "[core][bookmark]") {
    const auto result = make_bookmark({.url = "about:blank", .title = "", .description = ""});
    REQUIRE(result.has_value());
    CHECK(result->title.empty());
    CHECK(result->description.empty());
    CHECK(result->tags.empty());
}

TEST_CASE("make_bookmark accepts any valid UTF-8 text without a length limit", "[core][bookmark]") {
    const std::string long_text(100000, 'x');
    const std::string text = "\xC3\xB1\xE2\x82\xAC\xF0\x9F\x98\x80\t\x01";
    const auto result =
        make_bookmark({.url = "about:blank", .title = text, .description = long_text});
    REQUIRE(result.has_value());
    CHECK(result->title == text);
    CHECK(result->description == long_text);
}

TEST_CASE("make_bookmark refuses invalid UTF-8 and names the field", "[core][bookmark]") {
    const auto title =
        make_bookmark({.url = "about:blank", .title = "bad\xC3", .description = "ok"});
    REQUIRE_FALSE(title.has_value());
    CHECK(title.error().field == Field::kTitle);
    CHECK(title.error().reason == Reason::kInvalidUtf8);
    CHECK(title.error().value == "bad\xC3");

    const auto description =
        make_bookmark({.url = "about:blank", .title = "ok", .description = "\xFF"});
    REQUIRE_FALSE(description.has_value());
    CHECK(description.error().field == Field::kDescription);
    CHECK(description.error().reason == Reason::kInvalidUtf8);
}

TEST_CASE("make_bookmark refuses an invalid URL and names the field", "[core][bookmark]") {
    const auto result = make_bookmark({.url = "example.org", .title = "t", .description = "d"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kUrl);
    CHECK(result.error().reason == Reason::kNoScheme);
}

TEST_CASE("make_bookmark reports the URL before the title and description", "[core][bookmark]") {
    const auto result = make_bookmark({.url = "", .title = "\xFF", .description = "\xFF"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kUrl);
}

TEST_CASE("make_bookmark parses the raw tag text", "[core][bookmark]") {
    const auto result = make_bookmark({.url = "about:blank",
                                       .title = "t",
                                       .description = "d",
                                       .tags = "Linux, cli   tools,,LINUX"});
    REQUIRE(result.has_value());
    CHECK(result->tags == TagSet{"cli tools", "linux"});
}

TEST_CASE("make_bookmark returns the tag error", "[core][bookmark]") {
    const auto control = make_bookmark({.url = "about:blank",
                                        .title = "t",
                                        .description = "d",
                                        .tags = "ok,a\x01"
                                                "b"});
    REQUIRE_FALSE(control.has_value());
    CHECK(control.error().field == Field::kTag);
    CHECK(control.error().reason == Reason::kControlCharacter);

    const auto too_long = make_bookmark(
        {.url = "about:blank", .title = "t", .description = "d", .tags = std::string(129, 'a')});
    REQUIRE_FALSE(too_long.has_value());
    CHECK(too_long.error().field == Field::kTag);
    CHECK(too_long.error().reason == Reason::kTooLong);

    const auto bad_utf8 =
        make_bookmark({.url = "about:blank", .title = "t", .description = "d", .tags = "\xFF"});
    REQUIRE_FALSE(bad_utf8.has_value());
    CHECK(bad_utf8.error().reason == Reason::kInvalidUtf8);
}

TEST_CASE("make_bookmark reports the title and description before the tags", "[core][bookmark]") {
    const auto result =
        make_bookmark({.url = "about:blank", .title = "\xFF", .description = "d", .tags = "\xFF"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kTitle);
}

TEST_CASE("make_bookmark gives each named value to the field of the same name",
          "[core][bookmark]") {
    const auto result = make_bookmark(
        {.url = "https://one.example/", .title = "two", .description = "three", .tags = "four"});
    REQUIRE(result.has_value());
    CHECK(result->url == "https://one.example/");
    CHECK(result->title == "two");
    CHECK(result->description == "three");
    CHECK(result->tags == TagSet{"four"});
}

TEST_CASE("make_bookmark gives no tags when the tag text is left out or named empty",
          "[core][bookmark]") {
    const auto left_out = make_bookmark({.url = "about:blank", .title = "t", .description = "d"});
    REQUIRE(left_out.has_value());
    CHECK(left_out->tags.empty());

    const auto empty =
        make_bookmark({.url = "about:blank", .title = "t", .description = "d", .tags = ""});
    REQUIRE(empty.has_value());
    CHECK(empty->tags.empty());
}

TEST_CASE("make_bookmark reports the title before the description", "[core][bookmark]") {
    const auto result =
        make_bookmark({.url = "about:blank", .title = "\xFF", .description = "\xFF"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kTitle);
}

TEST_CASE("make_bookmark reports the description before the tags", "[core][bookmark]") {
    const auto result =
        make_bookmark({.url = "about:blank", .title = "t", .description = "\xFF", .tags = "\xFF"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kDescription);
}
