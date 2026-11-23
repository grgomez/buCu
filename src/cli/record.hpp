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
// line ends in LF. With `colour`, each part has its colour and a reset before its LF; without it,
// the block holds no escape sequence. Control characters in the title and description are written
// as escapes.
std::string format_record(core::BookmarkId id, const core::Bookmark& bookmark, bool colour);

// Prints the record of each bookmark `selection` covers, in `colour` or plain, with one empty line
// between records, one at a time as they are read, and returns how many it printed. Stops early,
// with `out` bad, when writing fails. Fails as `db::for_each_bookmark` does.
std::expected<std::int64_t, db::Error> print_records(db::Database& database,
                                                     const core::IdSelection& selection,
                                                     std::ostream& out, bool colour);

// Prints the record of each of the `count` bookmarks with the highest IDs, as `print_records`
// does, and returns how many it printed. Fails as `db::for_each_last_bookmark` does.
std::expected<std::int64_t, db::Error> print_last_records(db::Database& database,
                                                          std::int64_t count, std::ostream& out,
                                                          bool colour);

}  // namespace bucu::cli
