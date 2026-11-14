#pragma once

#include <cstdint>
#include <expected>

#include "bucu/core/bookmark.hpp"
#include "bucu/db/database.hpp"
#include "bucu/db/error.hpp"

namespace bucu::db {

// Stores `bookmark` and its tags in one transaction and returns the new ID. `bookmark` is already
// validated (`core::make_bookmark`); `now` is the Unix time in seconds, UTC, for `created_at` and
// `updated_at`. When the URL is already stored, nothing changes and the error is `kDuplicateUrl`
// with the ID that holds it (docs/product.md section 4, URL).
std::expected<std::int64_t, Error> insert_bookmark(Database& database,
                                                   const core::Bookmark& bookmark,
                                                   std::int64_t now);

}  // namespace bucu::db
