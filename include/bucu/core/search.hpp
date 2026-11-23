#pragma once

#include <expected>
#include <string>
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

}  // namespace bucu::core
