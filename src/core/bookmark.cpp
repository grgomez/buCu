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

std::expected<Bookmark, ValidationError> make_bookmark(const BookmarkFields& fields) {
    const std::string_view url = fields.url.value;
    const std::string_view title = fields.title.value;
    const std::string_view description = fields.description.value;
    const std::string_view tags = fields.tags;
    auto normalized = normalize_url(url);
    if (!normalized) {
        return std::unexpected(std::move(normalized.error()));
    }
    if (!is_valid_utf8(title)) {
        return std::unexpected(ValidationError{
            .field = Field::kTitle, .reason = Reason::kInvalidUtf8, .value = std::string(title)});
    }
    if (!is_valid_utf8(description)) {
        return std::unexpected(ValidationError{.field = Field::kDescription,
                                               .reason = Reason::kInvalidUtf8,
                                               .value = std::string(description)});
    }
    auto parsed_tags = parse_tags(tags);
    if (!parsed_tags) {
        return std::unexpected(std::move(parsed_tags.error()));
    }
    return Bookmark{.url = std::move(*normalized),
                    .title = std::string(title),
                    .description = std::string(description),
                    .tags = std::move(*parsed_tags),
                    .flags = FlagSet()};
}

}  // namespace bucu::core
