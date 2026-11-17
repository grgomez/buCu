#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/database.hpp"
#include "bucu/db/error.hpp"

namespace bucu::db {

// Stores `bookmark` and its tags in one transaction and returns the new ID. `bookmark` is already
// validated (`core::make_bookmark`); `now` is the time stored as `created_at` and
// `updated_at`, in Unix seconds, UTC. When the URL is already stored, nothing changes and the error
// is `kDuplicateUrl` with the ID that holds it (docs/product.md section 4, URL).
std::expected<core::BookmarkId, Error> insert_bookmark(Database& database,
                                                       const core::Bookmark& bookmark,
                                                       std::chrono::sys_seconds now);

// How many bookmarks are stored.
[[nodiscard]] std::expected<std::int64_t, Error> count_bookmarks(Database& database);

// Receives each stored bookmark with its ID. Returning `false` stops the walk.
using BookmarkVisitor = std::function<bool(core::BookmarkId, const core::Bookmark&)>;

// Calls `visit` for each stored bookmark that `selection` covers, with its tags, in ascending ID
// order, in one read transaction. An ID that is not stored is skipped. Nothing is buffered, so a
// range such as 1-9999999999 costs one record at a time.
[[nodiscard]] std::expected<void, Error> for_each_bookmark(Database& database,
                                                           const core::IdSelection& selection,
                                                           const BookmarkVisitor& visit);

}  // namespace bucu::db
