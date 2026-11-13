#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "bucu/core/error.hpp"
#include "bucu/core/tags.hpp"

namespace bucu::core {

// Flag bits (product section 4). The value is the bit Buku uses, so a database read from Buku
// keeps its meaning. Bits BuCu does not know are never changed.
inline constexpr std::int64_t kFlagImmutableTitle = 1;

// A bookmark without its database ID: the ID belongs to the stored record (#10).
struct Bookmark {
    std::string url;
    std::string title;
    std::string description;
    TagSet tags;
    std::int64_t flags = 0;
};

// Whether every bit of `flag` is set in `flags`.
bool has_flag(std::int64_t flags, std::int64_t flag);

// `flags` with the bits of `flag` set or cleared, and every other bit unchanged.
std::int64_t set_flag(std::int64_t flags, std::int64_t flag);
std::int64_t clear_flag(std::int64_t flags, std::int64_t flag);

// A new bookmark with no flag set. The URL is validated and normalized (product section 4); the
// title and description must be valid UTF-8 and are stored as given. `tags` come from parse_tags.
std::expected<Bookmark, ValidationError> make_bookmark(std::string_view url, std::string_view title,
                                                       std::string_view description,
                                                       TagSet tags = {});

}  // namespace bucu::core
