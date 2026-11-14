#pragma once

#include <chrono>
#include <expected>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
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

}  // namespace bucu::db
