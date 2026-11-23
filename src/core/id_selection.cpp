#include "bucu/core/id_selection.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace bucu::core {

namespace {

constexpr std::int64_t kLargestId = std::numeric_limits<std::int64_t>::max();

// A positive whole number of ASCII digits (leading zeros allowed) that fits in 64 bits.
std::optional<std::int64_t> parse_id(std::string_view text) {
    const bool all_digits =
        !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
    if (!all_digits) {
        return std::nullopt;
    }
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 1) {
        return std::nullopt;
    }
    return value;
}

// One list item: `<n>` or `<a>-<b>`.
std::optional<IdItem> parse_item(std::string_view text) {
    const std::size_t dash = text.find('-');
    if (dash == std::string_view::npos) {
        const auto id = parse_id(text);
        if (!id) {
            return std::nullopt;
        }
        return IdItem{.kind = IdItemKind::kSingle,
                      .range = IdRange{.first = BookmarkId(*id), .last = BookmarkId(*id)}};
    }
    const auto first = parse_id(text.substr(0, dash));
    const auto last = parse_id(text.substr(dash + 1));
    if (!first || !last) {
        return std::nullopt;
    }
    return IdItem{.kind = IdItemKind::kRange,
                  .range = IdRange{.first = BookmarkId(std::min(*first, *last)),
                                   .last = BookmarkId(std::max(*first, *last))}};
}

// Sort order of the items: low end, then a single before a range, then high end.
bool item_before(const IdItem& left, const IdItem& right) {
    if (left.range.first != right.range.first) {
        return left.range.first < right.range.first;
    }
    if (left.kind != right.kind) {
        return left.kind < right.kind;
    }
    return left.range.last < right.range.last;
}

}  // namespace

std::expected<IdSelection, IdError> parse_id_selection(const std::vector<std::string>& arguments) {
    IdSelection selection{.all = arguments.empty(), .items = {}};
    for (const std::string& argument : arguments) {
        std::size_t start = 0;
        while (start <= argument.size()) {
            std::size_t comma = argument.find(',', start);
            if (comma == std::string::npos) {
                comma = argument.size();
            }
            const auto item = parse_item(std::string_view(argument).substr(start, comma - start));
            if (!item) {
                return std::unexpected(IdError{.argument = argument});
            }
            selection.items.push_back(*item);
            start = comma + 1;
        }
    }
    std::ranges::sort(selection.items, item_before);
    const auto repeats = std::ranges::unique(selection.items);
    selection.items.erase(repeats.begin(), repeats.end());
    return selection;
}

IdSelection select_two_ids(BookmarkId a, BookmarkId b) {
    const auto single = [](BookmarkId id) {
        return IdItem{.kind = IdItemKind::kSingle, .range = IdRange{.first = id, .last = id}};
    };
    return IdSelection{.all = false, .items = {single(std::min(a, b)), single(std::max(a, b))}};
}

std::expected<BookmarkId, SingleIdError> parse_single_id(std::string_view text) {
    const auto selection = parse_id_selection({std::string(text)});
    if (!selection) {
        return std::unexpected(SingleIdError::kNotAnIdItem);
    }
    if (text.find_first_of(",-") != std::string_view::npos) {
        return std::unexpected(SingleIdError::kListOrRange);
    }
    return selection->items.front().range.first;
}

std::vector<IdRange> covered_ranges(const IdSelection& selection) {
    if (selection.all) {
        return {IdRange{.first = BookmarkId(1), .last = BookmarkId(kLargestId)}};
    }
    // The items are sorted by low end, so one pass merges the overlapping and touching ones.
    std::vector<IdRange> merged;
    for (const IdItem& item : selection.items) {
        const IdRange& next = item.range;
        // `last + 1` cannot overflow when `last` is below the largest ID, and the largest ID
        // touches nothing above it.
        if (!merged.empty() && (merged.back().last.value() == kLargestId ||
                                next.first.value() <= merged.back().last.value() + 1)) {
            merged.back().last = std::max(merged.back().last, next.last);
        } else {
            merged.push_back(next);
        }
    }
    return merged;
}

std::uint64_t named_id_count(const IdSelection& selection) {
    if (selection.all) {
        return 0;
    }
    std::uint64_t count = 0;
    for (const IdRange& range : covered_ranges(selection)) {
        count += static_cast<std::uint64_t>(range.last.value() - range.first.value()) + 1;
    }
    return count;
}

}  // namespace bucu::core
