#include "bucu/db/bookmarks.hpp"

#include <sqlite3.h>

#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "db/handle.hpp"
#include "db/statement.hpp"
#include "db/transaction.hpp"

namespace bucu::db {

namespace {

// The ID that holds `url`, or no value when the URL is not stored.
std::expected<std::optional<core::BookmarkId>, Error> find_by_url(sqlite3* connection,
                                                                  const std::string& url) {
    auto statement = Statement::prepare(connection, "SELECT id FROM bookmark WHERE url = ?");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }
    if (auto bound = statement->bind(1, url); !bound) {
        return std::unexpected(std::move(bound.error()));
    }
    auto row = statement->step();
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    if (!*row) {
        return std::nullopt;
    }
    return core::BookmarkId(statement->column_int(0));
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
    auto row = statement.step();
    // reset() only makes the statement ready again; the result of step() is the one that counts.
    std::ignore = statement.reset();
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    return {};
}

constexpr const char* kSelectTags =
    "SELECT tag.name FROM bookmark_tag JOIN tag ON tag.id = bookmark_tag.tag_id "
    "WHERE bookmark_tag.bookmark_id = ?";

// The tags that bookmark `id` links, read with the prepared `statement` (kSelectTags).
std::expected<core::TagSet, Error> read_tags(Statement& statement, core::BookmarkId id) {
    if (auto bound = statement.bind(1, id.value()); !bound) {
        return std::unexpected(std::move(bound.error()));
    }
    core::TagSet tags;
    for (;;) {
        auto row = statement.step();
        if (!row) {
            std::ignore = statement.reset();
            return std::unexpected(std::move(row.error()));
        }
        if (!*row) {
            break;
        }
        tags.insert(statement.column_text(0));
    }
    std::ignore = statement.reset();
    return tags;
}

}  // namespace

std::expected<core::BookmarkId, Error> insert_bookmark(Database& database,
                                                       const core::Bookmark& bookmark,
                                                       std::chrono::sys_seconds now) {
    const std::int64_t stored_time = now.time_since_epoch().count();  // Unix seconds
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection, /*immediate=*/true);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }

    auto existing = find_by_url(connection, bookmark.url);
    if (!existing) {
        return std::unexpected(std::move(existing.error()));
    }
    if (const std::optional<core::BookmarkId>& holder = *existing; holder.has_value()) {
        return std::unexpected(Error{
            .code = ErrorCode::kDuplicateUrl,
            .message = "the URL is already stored as bookmark " + std::to_string(holder->value()),
            .bookmark_id = holder});
    }

    auto insert = Statement::prepare(connection,
                                     "INSERT INTO bookmark "
                                     "(url, title, description, flags, created_at, updated_at) "
                                     "VALUES (?, ?, ?, ?, ?, ?)");
    if (!insert) {
        return std::unexpected(std::move(insert.error()));
    }
    if (auto done = run(*insert, bookmark.url, bookmark.title, bookmark.description,
                        bookmark.flags.bits(), stored_time, stored_time);
        !done) {
        return std::unexpected(std::move(done.error()));
    }
    const core::BookmarkId id(sqlite3_last_insert_rowid(connection));

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
        if (auto done = run(*link, id.value(), name); !done) {
            return std::unexpected(std::move(done.error()));
        }
    }

    if (auto committed = transaction->commit(); !committed) {
        return std::unexpected(std::move(committed.error()));
    }
    return id;
}

std::expected<std::int64_t, Error> count_bookmarks(Database& database) {
    auto statement = Statement::prepare(detail::handle(database), "SELECT count(*) FROM bookmark");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }
    auto row = statement->step();
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    return statement->column_int(0);
}

std::expected<void, Error> for_each_bookmark(Database& database, const core::IdSelection& selection,
                                             const BookmarkVisitor& visit) {
    sqlite3* const connection = detail::handle(database);
    // One read transaction, so every record comes from the same snapshot.
    auto transaction = Transaction::begin(connection);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    auto select = Statement::prepare(connection,
                                     "SELECT id, url, title, description, flags FROM bookmark "
                                     "WHERE id >= ? AND id <= ? ORDER BY id");
    if (!select) {
        return std::unexpected(std::move(select.error()));
    }
    auto select_tags = Statement::prepare(connection, kSelectTags);
    if (!select_tags) {
        return std::unexpected(std::move(select_tags.error()));
    }
    for (const core::IdRange& range : core::covered_ranges(selection)) {
        if (auto bound = select->bind(1, range.first.value()); !bound) {
            return std::unexpected(std::move(bound.error()));
        }
        if (auto bound = select->bind(2, range.last.value()); !bound) {
            return std::unexpected(std::move(bound.error()));
        }
        for (;;) {
            auto row = select->step();
            if (!row) {
                return std::unexpected(std::move(row.error()));
            }
            if (!*row) {
                break;
            }
            const core::BookmarkId id(select->column_int(0));
            auto tags = read_tags(*select_tags, id);
            if (!tags) {
                return std::unexpected(std::move(tags.error()));
            }
            const core::Bookmark bookmark{.url = select->column_text(1),
                                          .title = select->column_text(2),
                                          .description = select->column_text(3),
                                          .tags = std::move(*tags),
                                          .flags = core::FlagSet(select->column_int(4))};
            if (!visit(id, bookmark)) {
                return {};
            }
        }
        std::ignore = select->reset();
    }
    return {};
}

}  // namespace bucu::db
