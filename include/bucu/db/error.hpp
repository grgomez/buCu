#pragma once

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
};

struct Error {
    ErrorCode code;
    std::string message;
};

}  // namespace bucu::db
