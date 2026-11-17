#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/error.hpp"
#include "bucu/core/tags.hpp"

namespace bucu::core {

// How a tag change applies to a bookmark's tags (docs/product.md section 4, Tags).
enum class TagOperation : std::uint8_t {
    kAdd,
    kRemove,
    kReplace,
};

// The immutable-title flag, set or cleared (docs/product.md section 4, Flags).
enum class FlagChange : std::uint8_t {
    kSet,
    kClear,
};

// A tag change as the user gave it: `text` is raw tag input, split by parse_tags.
struct TagChange {
    TagOperation operation;
    std::string text;
};

// The changes a command asks for, as given. A field that is not set stays as stored; an empty
// title, description, or tag text clears it.
struct ChangeRequest {
    std::optional<std::string> url = std::nullopt;
    std::optional<std::string> title = std::nullopt;
    std::optional<std::string> description = std::nullopt;
    std::optional<TagChange> tags = std::nullopt;
    std::optional<FlagChange> flag = std::nullopt;
};

// A tag change whose tags are already checked.
struct CheckedTagChange {
    TagOperation operation;
    TagSet tags;
};

// The changes of a ChangeRequest after checking: the URL is normalized and every text is valid.
struct BookmarkChanges {
    std::optional<std::string> url = std::nullopt;
    std::optional<std::string> title = std::nullopt;
    std::optional<std::string> description = std::nullopt;
    std::optional<CheckedTagChange> tags = std::nullopt;
    std::optional<FlagChange> flag = std::nullopt;
};

// Checks the values of `request` (product section 4) and returns the first broken rule, checked in
// the order URL, title, description, tags.
[[nodiscard]] std::expected<BookmarkChanges, ValidationError> make_changes(
    const ChangeRequest& request);

// `bookmark` with `changes` applied: given fields replace the stored ones, the tag operation
// applies to the tags, and the flag change keeps every other flag bit.
[[nodiscard]] Bookmark apply_changes(Bookmark bookmark, const BookmarkChanges& changes);

}  // namespace bucu::core
