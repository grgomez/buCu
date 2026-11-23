#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/changes.hpp"
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
// order, in one read transaction. Fails with `kNotFound`, before the first visit, when a named
// ID is not stored or a range holds no stored ID; the first such item in ascending order of its
// low end is named. Every bookmark has no such check, so an empty database visits nothing. Nothing
// is buffered, so a range such as 1-9999999999 costs one record at a time.
[[nodiscard]] std::expected<void, Error> for_each_bookmark(Database& database,
                                                           const core::IdSelection& selection,
                                                           const BookmarkVisitor& visit);

// Calls `visit` for each of the `count` stored bookmarks with the highest IDs, with its tags, in
// ascending ID order, in one read transaction. When fewer than `count` are stored, it visits all of
// them; `count` below 1 or no bookmark stored visits none. It counts stored bookmarks, not ID
// values. Nothing is buffered.
[[nodiscard]] std::expected<void, Error> for_each_last_bookmark(Database& database,
                                                                std::int64_t count,
                                                                const BookmarkVisitor& visit);

// Applies `changes` to each bookmark that `selection` covers, in one transaction (all or nothing).
// Fails with `kNotFound` when a single ID is not stored, a range holds no stored ID, or, for every
// bookmark, none is stored; the first such item in ascending order of its low end is named. That
// check comes before the URL check: `kDuplicateUrl` when another bookmark holds `changes.url`
// (a bookmark that already has the URL is no conflict). Only a bookmark whose values change is
// written: it gets `updated_at = now` and keeps `created_at`. Tags that no bookmark links any more
// are deleted before the commit (docs/database-schema.md section 3).
[[nodiscard]] std::expected<void, Error> update_bookmarks(Database& database,
                                                          const core::IdSelection& selection,
                                                          const core::BookmarkChanges& changes,
                                                          std::chrono::sys_seconds now);

// Checks that `selection` matches stored bookmarks, in one read transaction: fails with the
// `kNotFound` error that `delete_bookmarks` gives, and changes nothing.
[[nodiscard]] std::expected<void, Error> check_bookmarks_exist(Database& database,
                                                               const core::IdSelection& selection);

// Deletes each bookmark that `selection` covers, with its tag links, in one transaction (all or
// nothing), and returns how many bookmarks it deleted. No other bookmark changes its ID. Fails with
// `kNotFound`, as `update_bookmarks` does, before it deletes anything. Tags that no bookmark links
// any more are deleted before the commit (docs/database-schema.md section 3).
[[nodiscard]] std::expected<std::int64_t, Error> delete_bookmarks(
    Database& database, const core::IdSelection& selection);

// Exchanges the IDs of bookmarks `a` and `b` in one transaction (all or nothing): each bookmark
// keeps every field, tag, flag, and timestamp, and `bookmark_tag` follows the new IDs. Fails with
// `kNotFound` naming the lower of `a` and `b` that is not stored, and changes nothing. `a` and `b`
// differ.
[[nodiscard]] std::expected<void, Error> swap_bookmark_ids(Database& database, core::BookmarkId a,
                                                           core::BookmarkId b);

// How many stored bookmarks have an ID other than their place (1, 2, ...) in ascending ID order,
// in one read transaction. 0 means the IDs are already 1 to n.
[[nodiscard]] std::expected<std::int64_t, Error> count_misplaced_ids(Database& database);

// Gives the stored bookmarks the IDs 1 to n in their current ascending ID order, in one
// transaction (all or nothing), and returns how many bookmarks got a new ID. Each bookmark keeps
// every field, tag, flag, and timestamp; `bookmark_tag` follows the new IDs.
[[nodiscard]] std::expected<std::int64_t, Error> renumber_bookmarks(Database& database);

}  // namespace bucu::db
