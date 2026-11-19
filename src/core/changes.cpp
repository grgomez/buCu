#include "bucu/core/changes.hpp"

#include <expected>
#include <optional>
#include <string>
#include <utility>

#include "bucu/core/tags.hpp"
#include "bucu/core/url.hpp"
#include "core/utf8.hpp"

namespace bucu::core {

namespace {

// The error for a title or description that is not valid UTF-8, the only rule they have.
std::optional<ValidationError> check_text(Field field, const std::optional<std::string>& text) {
    if (text.has_value() && !is_valid_utf8(*text)) {
        return ValidationError{.field = field, .reason = Reason::kInvalidUtf8, .value = *text};
    }
    return std::nullopt;
}

}  // namespace

std::expected<BookmarkChanges, ValidationError> make_changes(const ChangeRequest& request) {
    BookmarkChanges changes;
    changes.flag = request.flag;
    if (request.url.has_value()) {
        auto url = normalize_url(*request.url);
        if (!url) {
            return std::unexpected(std::move(url.error()));
        }
        changes.url = std::move(*url);
    }
    if (auto error = check_text(Field::kTitle, request.title)) {
        return std::unexpected(std::move(*error));
    }
    changes.title = request.title;
    if (auto error = check_text(Field::kDescription, request.description)) {
        return std::unexpected(std::move(*error));
    }
    changes.description = request.description;
    if (request.tags.has_value()) {
        auto tags = parse_tags(request.tags->text);
        if (!tags) {
            return std::unexpected(std::move(tags.error()));
        }
        changes.tags =
            CheckedTagChange{.operation = request.tags->operation, .tags = std::move(*tags)};
    }
    return changes;
}

Bookmark apply_changes(Bookmark bookmark, const BookmarkChanges& changes) {
    if (changes.url.has_value()) {
        bookmark.url = *changes.url;
    }
    if (changes.title.has_value()) {
        bookmark.title = *changes.title;
    }
    if (changes.description.has_value()) {
        bookmark.description = *changes.description;
    }
    if (changes.tags.has_value()) {
        switch (changes.tags->operation) {
            case TagOperation::kAdd:
                bookmark.tags = add_tags(std::move(bookmark.tags), changes.tags->tags);
                break;
            case TagOperation::kRemove:
                bookmark.tags = remove_tags(std::move(bookmark.tags), changes.tags->tags);
                break;
            case TagOperation::kReplace:
                bookmark.tags = changes.tags->tags;
                break;
        }
    }
    if (changes.flag.has_value()) {
        bookmark.flags = *changes.flag == FlagChange::kSet
                             ? set_flag(bookmark.flags, kFlagImmutableTitle)
                             : clear_flag(bookmark.flags, kFlagImmutableTitle);
    }
    return bookmark;
}

}  // namespace bucu::core
