#pragma once

#include <compare>
#include <cstdint>

namespace bucu::core {

// The ID of a stored bookmark (docs/product.md section 2). It is a type of its own, so an ID cannot
// be mixed up with a count, a flag set, or a time: it is made only on purpose, from the number a
// database gives, and has no arithmetic and no implicit conversion.
class BookmarkId {
public:
    explicit constexpr BookmarkId(std::int64_t value) noexcept : value_(value) {}

    constexpr std::int64_t value() const noexcept { return value_; }

    friend constexpr auto operator<=>(const BookmarkId&, const BookmarkId&) = default;

private:
    std::int64_t value_;
};

}  // namespace bucu::core
