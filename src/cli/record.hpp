#pragma once

#include <cstdint>
#include <string>

#include "bucu/core/bookmark.hpp"

namespace bucu::cli {

// The labelled block that `add` prints for a stored bookmark (docs/product.md section 5). Every
// line ends in LF.
std::string format_record(std::int64_t id, const core::Bookmark& bookmark);

}  // namespace bucu::cli
