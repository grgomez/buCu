#include "bucu/core/bookmark_id.hpp"

#include <compare>
#include <cstdint>
#include <type_traits>

#include "catch_amalgamated.hpp"

namespace {

using bucu::core::BookmarkId;

// An ID is not a plain number: it cannot be made from one, or become one, without saying so.
static_assert(!std::is_convertible_v<std::int64_t, BookmarkId>);
static_assert(!std::is_convertible_v<BookmarkId, std::int64_t>);
static_assert(std::is_constructible_v<BookmarkId, std::int64_t>);
static_assert(!std::is_default_constructible_v<BookmarkId>);

template <typename Left, typename Right>
concept CanAdd = requires(Left left, Right right) { left + right; };
static_assert(!CanAdd<BookmarkId, BookmarkId>);
static_assert(!CanAdd<BookmarkId, int>);

}  // namespace

TEST_CASE("a BookmarkId holds the number it was made from", "[core]") {
    CHECK(BookmarkId(7).value() == 7);
    CHECK(BookmarkId(9223372036854775807).value() == 9223372036854775807);
}

TEST_CASE("BookmarkIds compare by their numbers", "[core]") {
    CHECK(BookmarkId(3) == BookmarkId(3));
    CHECK(BookmarkId(3) != BookmarkId(4));
    CHECK(BookmarkId(3) < BookmarkId(4));
    CHECK(BookmarkId(5) > BookmarkId(4));
    CHECK((BookmarkId(3) <=> BookmarkId(3)) == std::strong_ordering::equal);
}
