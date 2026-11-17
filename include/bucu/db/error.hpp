#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>

#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"

namespace bucu::db {

// A schema version (docs/database-schema.md section 2). It is a type of its own, so a version
// cannot be mixed up with a count or an ID: it is made only on purpose, and has no arithmetic and
// no implicit conversion.
class SchemaVersion {
public:
    explicit constexpr SchemaVersion(int value) noexcept : value_(value) {}

    [[nodiscard]] constexpr int value() const noexcept { return value_; }

    friend constexpr auto operator<=>(const SchemaVersion&, const SchemaVersion&) = default;

private:
    int value_;
};

// The newest schema version this BuCu supports.
inline constexpr SchemaVersion kSchemaVersion{1};

// Why a database operation failed. `cli` maps each code to a message and an exit code.
enum class ErrorCode : std::uint8_t {
    kCannotOpen,
    kNotADatabase,
    kNotBucuDatabase,
    kNewerSchema,
    kMigrationFailed,
    kSql,
    kDuplicateUrl,
    kDestinationExists,
    kNotFound,
};

struct Error {
    ErrorCode code;
    std::string message;
    // kDuplicateUrl: the ID that holds the URL. kNotFound: the single ID that is not stored.
    std::optional<core::BookmarkId> bookmark_id;
    // kNotFound: the range that holds no stored ID. Neither field is set when no bookmark exists.
    std::optional<core::IdRange> range;
};

}  // namespace bucu::db
