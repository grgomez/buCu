#pragma once

#include <expected>
#include <filesystem>
#include <memory>

#include "bucu/db/error.hpp"

struct sqlite3;

namespace bucu::db {

class Database;

namespace detail {
class Access;  // db module internals reach the connection through it (src/db/handle.hpp)
}  // namespace detail

// One open connection to a BuCu database file. Closes the connection when destroyed.
class Database {
public:
    // Opens the database at `path`, creating it when no file exists there.
    static std::expected<Database, Error> open(const std::filesystem::path& path);

    Database(Database&&) noexcept = default;
    Database& operator=(Database&&) noexcept = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    ~Database() = default;

    // The schema version of the open database: `kSchemaVersion` after a successful `open`.
    int schema_version() const noexcept { return schema_version_; }

private:
    struct Closer {
        void operator()(sqlite3* connection) const noexcept;
    };

    explicit Database(sqlite3* connection) : connection_(connection) {}

    friend class detail::Access;

    std::unique_ptr<sqlite3, Closer> connection_;
    int schema_version_ = 0;
};

}  // namespace bucu::db
