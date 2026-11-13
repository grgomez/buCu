#include "bucu/core/bookmark.hpp"

#include <cstdint>

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
