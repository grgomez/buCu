#include "bucu/core/bookmark.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "bucu/core/url.hpp"
#include "core/utf8.hpp"

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

std::expected<Bookmark, ValidationError> make_bookmark(std::string_view url, std::string_view title,
                                                       std::string_view description, TagSet tags) {
    auto normalized = normalize_url(url);
    if (!normalized) {
        return std::unexpected(std::move(normalized.error()));
    }
    if (!is_valid_utf8(title)) {
        return std::unexpected(
            ValidationError{Field::kTitle, Reason::kInvalidUtf8, std::string(title)});
    }
    if (!is_valid_utf8(description)) {
        return std::unexpected(
            ValidationError{Field::kDescription, Reason::kInvalidUtf8, std::string(description)});
    }
    return Bookmark{std::move(*normalized), std::string(title), std::string(description),
                    std::move(tags), 0};
}

}  // namespace bucu::core
