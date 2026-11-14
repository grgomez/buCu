#include "bucu/core/bookmark.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "bucu/core/url.hpp"
#include "core/utf8.hpp"

namespace bucu::core {

namespace {

// Bit operations run on unsigned values, so the high bit of a stored flags value is no hazard.
std::uint64_t unsigned_bits(std::int64_t value) { return static_cast<std::uint64_t>(value); }

}  // namespace

bool has_flag(FlagSet flags, Flag flag) {
    return (unsigned_bits(flags.bits()) & unsigned_bits(flag.bits())) == unsigned_bits(flag.bits());
}

FlagSet set_flag(FlagSet flags, Flag flag) {
    return FlagSet(
        static_cast<std::int64_t>(unsigned_bits(flags.bits()) | unsigned_bits(flag.bits())));
}

FlagSet clear_flag(FlagSet flags, Flag flag) {
    return FlagSet(
        static_cast<std::int64_t>(unsigned_bits(flags.bits()) & ~unsigned_bits(flag.bits())));
}

std::expected<Bookmark, ValidationError> make_bookmark(std::string_view url, std::string_view title,
                                                       std::string_view description,
                                                       std::string_view tags) {
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
    auto parsed_tags = parse_tags(tags);
    if (!parsed_tags) {
        return std::unexpected(std::move(parsed_tags.error()));
    }
    return Bookmark{std::move(*normalized), std::string(title), std::string(description),
                    std::move(*parsed_tags), FlagSet()};
}

}  // namespace bucu::core
