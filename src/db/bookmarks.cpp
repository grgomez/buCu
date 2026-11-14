#include "bucu/db/bookmarks.hpp"

#include <sqlite3.h>

#include <optional>
#include <string>
#include <utility>

#include "db/handle.hpp"
#include "db/statement.hpp"
#include "db/transaction.hpp"

namespace bucu::db {

namespace {

// The ID that holds `url`, or no value when the URL is not stored.
std::expected<std::optional<std::int64_t>, Error> find_by_url(sqlite3* connection,
                                                              const std::string& url) {
    auto statement = Statement::prepare(connection, "SELECT id FROM bookmark WHERE url = ?");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }
    if (auto bound = statement->bind(1, url); !bound) {
        return std::unexpected(std::move(bound.error()));
    }
    const auto row = statement->step();
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    if (!*row) {
        return std::nullopt;
    }
    return statement->column_int(0);
}

// Runs a prepared statement that returns no row, after binding `values` to parameters 1, 2, ...
template <typename... Values>
std::expected<void, Error> run(Statement& statement, const Values&... values) {
    int index = 0;
    std::expected<void, Error> bound;
    // Binds in order and stops at the first failure, so that failure is the error returned.
    const auto bind_next = [&](const auto& value) {
        if (bound) {
            bound = statement.bind(++index, value);
        }
    };
    (bind_next(values), ...);
    if (!bound) {
        return bound;
    }
    const auto row = statement.step();
    // reset() only makes the statement ready again; the result of step() is the one that counts.
    static_cast<void>(statement.reset());
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    return {};
}

}  // namespace

std::expected<std::int64_t, Error> insert_bookmark(Database& database,
                                                   const core::Bookmark& bookmark,
                                                   std::int64_t now) {
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection, /*immediate=*/true);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }

    const auto existing = find_by_url(connection, bookmark.url);
    if (!existing) {
        return std::unexpected(std::move(existing.error()));
    }
    if (existing->has_value()) {
        return std::unexpected(Error{
            ErrorCode::kDuplicateUrl,
            "the URL is already stored as bookmark " + std::to_string(**existing), *existing});
    }

    auto insert = Statement::prepare(connection,
                                     "INSERT INTO bookmark "
                                     "(url, title, description, flags, created_at, updated_at) "
                                     "VALUES (?, ?, ?, ?, ?, ?)");
    if (!insert) {
        return std::unexpected(std::move(insert.error()));
    }
    if (auto done = run(*insert, bookmark.url, bookmark.title, bookmark.description, bookmark.flags,
                        now, now);
        !done) {
        return std::unexpected(std::move(done.error()));
    }
    const std::int64_t id = sqlite3_last_insert_rowid(connection);

    auto add_tag = Statement::prepare(connection, "INSERT OR IGNORE INTO tag (name) VALUES (?)");
    auto link = Statement::prepare(connection,
                                   "INSERT INTO bookmark_tag (bookmark_id, tag_id) "
                                   "SELECT ?, id FROM tag WHERE name = ?");
    if (!add_tag) {
        return std::unexpected(std::move(add_tag.error()));
    }
    if (!link) {
        return std::unexpected(std::move(link.error()));
    }
    for (const std::string& name : bookmark.tags) {
        if (auto done = run(*add_tag, name); !done) {
            return std::unexpected(std::move(done.error()));
        }
        if (auto done = run(*link, id, name); !done) {
            return std::unexpected(std::move(done.error()));
        }
    }

    if (auto committed = transaction->commit(); !committed) {
        return std::unexpected(std::move(committed.error()));
    }
    return id;
}

}  // namespace bucu::db
