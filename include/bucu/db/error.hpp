#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace bucu::db {

// The newest schema version this BuCu supports (docs/database-schema.md section 2).
inline constexpr int kSchemaVersion = 1;

// Why a database operation failed. `cli` maps each code to a message and an exit code.
enum class ErrorCode {
    kCannotOpen,
    kNotADatabase,
    kNotBucuDatabase,
    kNewerSchema,
    kMigrationFailed,
    kSql,
    kDuplicateUrl,
    kDestinationExists,
};

struct Error {
    ErrorCode code;
    std::string message;
    std::optional<std::int64_t> bookmark_id;  // kDuplicateUrl: the ID that holds the URL
};

}  // namespace bucu::db
