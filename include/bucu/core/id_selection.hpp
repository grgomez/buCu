#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "bucu/core/bookmark_id.hpp"

namespace bucu::core {

// The IDs from `first` to `last`, both included, with `first <= last`.
struct IdRange {
    BookmarkId first;
    BookmarkId last;

    friend constexpr bool operator==(const IdRange&, const IdRange&) = default;
};

// One item of an ID argument: a single ID (`first == last`) or a range as the user wrote it.
enum class IdItemKind : std::uint8_t {
    kSingle,
    kRange,
};

struct IdItem {
    IdItemKind kind;
    IdRange range;

    friend constexpr bool operator==(const IdItem&, const IdItem&) = default;
};

// The bookmarks a command names (docs/product.md section 5, IDs and ranges): every bookmark when
// no ID was given, or the items. `items` is sorted by the low end of each item (a single ID before
// a range with the same low end) and holds no item twice. It is empty when `all` is true.
struct IdSelection {
    bool all = false;
    std::vector<IdItem> items;
};

// The argument that is not a positive whole number that fits in a signed 64-bit integer, as given.
struct IdError {
    std::string argument;
};

// Reads the ID arguments: each is a positive whole number, a range `<a>-<b>`, or a comma-separated
// list of these. A range with `a > b` is read as `<b>-<a>`. No argument selects every bookmark.
[[nodiscard]] std::expected<IdSelection, IdError> parse_id_selection(
    const std::vector<std::string>& arguments);

// The IDs the items name, as ascending ranges that neither overlap nor touch. For every bookmark,
// the one range from 1 to the largest ID. A range like `1-9999999999` stays one range.
[[nodiscard]] std::vector<IdRange> covered_ranges(const IdSelection& selection);

// How many distinct IDs the items name; 0 when the selection is every bookmark.
[[nodiscard]] std::uint64_t named_id_count(const IdSelection& selection);

}  // namespace bucu::core
