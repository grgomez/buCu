#pragma once

#include <cstdint>
#include <string>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"

namespace bucu::cli {

// The labelled block that `add` prints for a stored bookmark (docs/product.md section 5). Every
// line ends in LF.
std::string format_record(core::BookmarkId id, const core::Bookmark& bookmark);

}  // namespace bucu::cli
