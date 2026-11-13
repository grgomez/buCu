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

// The union of `tags` and `added`.
TagSet add_tags(TagSet tags, const TagSet& added);

// `tags` without the members of `removed`. A tag that is not in `tags` is ignored. Replacing
// the tags is assigning a new set, which may be empty.
TagSet remove_tags(TagSet tags, const TagSet& removed);

}  // namespace bucu::core
