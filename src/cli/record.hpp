#pragma once

#include <cstdint>
#include <expected>
#include <iosfwd>
#include <string>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"

namespace bucu::cli {

// The labelled block that `add` prints for a stored bookmark (docs/product.md section 5). Every
// line ends in LF.
std::string format_record(core::BookmarkId id, const core::Bookmark& bookmark);

// Prints the record of each bookmark `selection` covers, with one empty line between records, and
// returns how many it printed. Stops early, with `out` bad, when writing fails.
std::expected<std::int64_t, db::Error> print_records(db::Database& database,
                                                     const core::IdSelection& selection,
                                                     std::ostream& out);

}  // namespace bucu::cli
