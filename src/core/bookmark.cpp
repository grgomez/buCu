#include "bucu/core/bookmark.hpp"

#include <cstdint>

namespace bucu::core {

namespace {

// Bit operations run on unsigned values, so the high bit of a stored flags value is no hazard.
std::uint64_t bits(std::int64_t value) { return static_cast<std::uint64_t>(value); }

}  // namespace

bool has_flag(std::int64_t flags, std::int64_t flag) {
    return (bits(flags) & bits(flag)) == bits(flag);
}

std::int64_t set_flag(std::int64_t flags, std::int64_t flag) {
    return static_cast<std::int64_t>(bits(flags) | bits(flag));
}

std::int64_t clear_flag(std::int64_t flags, std::int64_t flag) {
    return static_cast<std::int64_t>(bits(flags) & ~bits(flag));
}

}  // namespace bucu::core
