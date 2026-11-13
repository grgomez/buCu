# Module 002: The core domain model

- Kind: Module
- Version: 2 (from 1)
- Status: Proposed
- Date: 2026-11-12
- Issue: #9
- Previous record: [`module-001`](module-001-db-database.md)
- ADR: none

## Summary

The first public headers of the `core` module: the bookmark model, URL validation and normalization, tag parsing and set operations, the immutable-title flag, and `ValidationError`. `core` includes no SQLite, CLI11, or operating-system header.

## Module contract

- **Contract:** the bookmark domain rules (`include/bucu/core/error.hpp`, `url.hpp`, `tags.hpp`, `bookmark.hpp`), in namespace `bucu::core`. The rules are in `docs/product.md` §4.
- **Owner:** the `core` module.
- **Callers:** none yet; `cli` and `db` use it from #10.
- **Inputs:** `normalize_url(std::string_view)`; `parse_tags(std::string_view)` (comma-separated); `add_tags` and `remove_tags` (`TagSet`); `make_bookmark(url, title, description, TagSet)`; `has_flag`, `set_flag`, and `clear_flag` on a `std::int64_t` flags value with `kFlagImmutableTitle` (1). All text is UTF-8.
- **Outputs:** `std::expected<std::string, ValidationError>` (the normalized URL), `std::expected<TagSet, ValidationError>`, `std::expected<Bookmark, ValidationError>`. `TagSet` is `std::set<std::string>`, in ascending byte order. `Bookmark` holds URL, title, description, tags, and flags, with no ID.
- **Errors:** `ValidationError` holds a `Field` (`kUrl`, `kTag`, `kTitle`, `kDescription`), a `Reason` (`kEmpty`, `kInvalidUtf8`, `kNoScheme`, `kWhitespace`, `kControlCharacter`, `kEmptyHost`, `kTooLong`), and the offending value; it holds no message text. `cli` maps each `Reason` to a message (product §6, #10).
- **Tests:** `tests/core/url_test.cpp`, `tags_test.cpp`, and `bookmark_test.cpp`.
- **Change from the previous record:** first version of the `core` headers.
