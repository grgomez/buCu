#pragma once

#include <cstdint>
#include <string>

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

}  // namespace bucu::core
