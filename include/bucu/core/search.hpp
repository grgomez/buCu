#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "bucu/core/error.hpp"

namespace bucu::core {

// What a search looks for (docs/product.md section 5, search): the keywords, each non-empty,
// valid UTF-8, trimmed, with inner whitespace runs as one space, lowercased, and distinct, in the
// order given; and the two match switches.
struct SearchQuery {
    std::vector<std::string> keywords;
    bool all = false;   // every keyword must match, not at least one
    bool deep = false;  // a keyword matches inside words too, not only as a whole word
};

// Reads the keyword arguments: each argument is one keyword. An empty keyword is dropped and a
// repeated one counts once. A keyword that is not valid UTF-8 is `Field::kKeyword` with
// `Reason::kInvalidUtf8`; no keyword left is `Field::kKeyword` with `Reason::kEmpty`.
[[nodiscard]] std::expected<SearchQuery, ValidationError> parse_search_query(
    const std::vector<std::string>& arguments, bool all, bool deep);

// How many distinct keywords of `query` match at least one of `fields`: a bookmark's URL, title,
// description, and each tag, one entry each (product section 5, search). A keyword matches one
// field at a time, never across two. Both sides are lowercased, no other folding; a keyword
// matches inside the field when `query.deep`, and otherwise only as a whole word. A field that is
// not valid UTF-8 matches nothing. The keywords must come from `parse_search_query`.
[[nodiscard]] std::size_t count_matching_keywords(const SearchQuery& query,
                                                  std::span<const std::string_view> fields);

}  // namespace bucu::core
