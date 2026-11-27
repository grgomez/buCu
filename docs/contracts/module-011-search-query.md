# Module 011: Search query and keyword results

- Kind: Module
- Version: 11 (from 10)
- Status: Accepted
- Date: 2026-11-23
- Issue: #112
- Previous record: [`module-010`](module-010-list-queries.md)
- ADR: none

## Summary

`bucu search` needs a keyword query, a matcher, and a ranked stream of results. `core` gains `search.hpp` (`SearchQuery`, `parse_search_query`, `count_matching_keywords`) and `Field::kKeyword`; `db` gains `for_each_search_result`. Nothing is removed or renamed.

## Module contract

- **Contract:** the search rules of `core` and the ranked result stream of `db`.
- **Owner:** `core` (`include/bucu/core/search.hpp`, `include/bucu/core/error.hpp`) and `db` (`include/bucu/db/bookmarks.hpp`).
- **Callers:** `cli` (`search`), the tests, and the fuzz target.
- **Inputs:**
  - `core::parse_search_query(const std::vector<std::string>& arguments, bool all, bool deep)`: each argument is one keyword.
  - `core::count_matching_keywords(const SearchQuery&, std::span<const std::string_view> fields)`: the fields are a bookmark's URL, title, description, and each tag, one entry each. The keywords must come from `parse_search_query`.
  - `db::for_each_search_result(Database&, const core::SearchQuery&, const BookmarkVisitor&)`.
- **Outputs:**
  - `core::parse_search_query` returns `std::expected<SearchQuery, ValidationError>`, `[[nodiscard]]`. A `SearchQuery` holds the keywords (non-empty, valid UTF-8, trimmed, inner whitespace runs as one space, lowercased, distinct, in the order given) and the `all` and `deep` switches.
  - `core::count_matching_keywords` returns, `[[nodiscard]]`, how many distinct keywords match at least one field; a keyword never matches across two fields. Both sides are lowercased with the mapping of `core::to_lower`. Without `deep` a keyword matches only a whole word (Unicode general categories L and N, and `_`, are word characters); with `deep`, anywhere. A field that is not valid UTF-8 matches nothing.
  - `db::for_each_search_result` returns `std::expected<void, Error>`, `[[nodiscard]]`: visits each matching bookmark with its tags, by the number of distinct keywords matched (highest first), then in ascending ID order, in one read transaction, one row at a time. It matches when at least one keyword matches, or every keyword with `all`. It stops without error when the visitor returns `false`. While it runs the connection has the SQL function `bucu_keyword_score`, which it removes before returning.
- **Errors:**
  - `core::Field::kKeyword` is added to the `Field` enum, so every `switch` over `Field` must handle it. `parse_search_query` returns `Field::kKeyword` with `Reason::kInvalidUtf8` (value as given) for a keyword that is not valid UTF-8, and `Field::kKeyword` with `Reason::kEmpty` when no keyword is left.
  - `db::for_each_search_result` fails with `ErrorCode::kSql` when the function cannot be registered or a statement fails. `cli` maps `kEmpty` to a usage error (exit `2`), `kInvalidUtf8` to a validation error (exit `3`), and `kSql` to exit `1` (`docs/product.md` §6).
- **Tests:** `tests/core/word_characters_test.cpp`, `tests/core/search_test.cpp`, `tests/core/search_property_test.cpp`, `tests/db/search_test.cpp`, `tests/cli/search_test.cpp`, and the fuzz target `tests/fuzz/search_fuzz.cpp`.
- **Change from the previous record:** `core/search.hpp` is added; `Field::kKeyword` is added to `core/error.hpp`; `db::for_each_search_result` is added to `db/bookmarks.hpp`. Nothing is removed or renamed.
