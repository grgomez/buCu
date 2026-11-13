#pragma once

#include <expected>
#include <set>
#include <string>
#include <string_view>

#include "bucu/core/error.hpp"

namespace bucu::core {

// A bookmark's tags: no duplicates, in ascending byte order of the normalized tag. The default
// comparison of std::string orders by unsigned byte.
using TagSet = std::set<std::string>;

// Parse comma-separated tag input (product section 4): trim, collapse inner whitespace, drop
// empty tags, lowercase, and deduplicate; or return the rule a tag broke.
std::expected<TagSet, ValidationError> parse_tags(std::string_view input);

}  // namespace bucu::core
