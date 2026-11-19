#include "bucu/db/bookmarks.hpp"

#include <sqlite3.h>

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
    // Unused when `values` is empty.
    [[maybe_unused]] const auto bind_next = [&](const auto& value) {
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

// The error for an ID or range that no stored bookmark matches. With neither, no bookmark exists.
std::unexpected<Error> not_found(std::optional<core::BookmarkId> id,
                                 std::optional<core::IdRange> range, std::string message) {
    return std::unexpected(Error{.code = ErrorCode::kNotFound,
                                 .message = std::move(message),
                                 .bookmark_id = id,
                                 .range = range});
}

// Whether a bookmark is stored in `range`, asked with the prepared `exists` statement.
std::expected<bool, Error> any_stored_in(Statement& exists, const core::IdRange& range) {
    if (auto bound = exists.bind(1, range.first.value()); !bound) {
        return std::unexpected(std::move(bound.error()));
    }
    if (auto bound = exists.bind(2, range.last.value()); !bound) {
        return std::unexpected(std::move(bound.error()));
    }
    auto row = exists.step();
    std::ignore = exists.reset();
    return row;
}

// Checks that every item of `selection` matches a stored bookmark, or that one is stored when
// `selection` is every bookmark. The items are in ascending order of their low end, so the first
// failure is the one to report.
std::expected<void, Error> check_targets_exist(sqlite3* connection,
                                               const core::IdSelection& selection) {
    auto exists =
        Statement::prepare(connection, "SELECT 1 FROM bookmark WHERE id >= ? AND id <= ? LIMIT 1");
    if (!exists) {
        return std::unexpected(std::move(exists.error()));
    }
    if (selection.all) {
        const auto found = any_stored_in(*exists, core::covered_ranges(selection).front());
        if (!found) {
            return std::unexpected(found.error());
        }
        if (!*found) {
            return not_found(std::nullopt, std::nullopt, "no bookmark is stored");
        }
        return {};
    }
    for (const core::IdItem& item : selection.items) {
        const auto found = any_stored_in(*exists, item.range);
        if (!found) {
            return std::unexpected(found.error());
        }
        if (*found) {
            continue;
        }
        if (item.kind == core::IdItemKind::kSingle) {
            return not_found(item.range.first, std::nullopt,
                             "no bookmark has ID " + std::to_string(item.range.first.value()));
        }
        return not_found(std::nullopt, item.range, "no bookmark has an ID in the range");
    }
    return {};
}

// The statements that write one changed bookmark, prepared once per command.
class RowWriter {
public:
    static std::expected<RowWriter, Error> prepare(sqlite3* connection) {
        auto update = Statement::prepare(connection,
                                         "UPDATE bookmark SET url = ?, title = ?, description = ?, "
                                         "flags = ?, updated_at = ? WHERE id = ?");
        if (!update) {
            return std::unexpected(std::move(update.error()));
        }
        auto unlink = Statement::prepare(connection,
                                         "DELETE FROM bookmark_tag WHERE bookmark_id = ? "
                                         "AND tag_id = (SELECT id FROM tag WHERE name = ?)");
        if (!unlink) {
            return std::unexpected(std::move(unlink.error()));
        }
        auto add_tag =
            Statement::prepare(connection, "INSERT OR IGNORE INTO tag (name) VALUES (?)");
        if (!add_tag) {
            return std::unexpected(std::move(add_tag.error()));
        }
        auto link = Statement::prepare(connection,
                                       "INSERT INTO bookmark_tag (bookmark_id, tag_id) "
                                       "SELECT ?, id FROM tag WHERE name = ?");
        if (!link) {
            return std::unexpected(std::move(link.error()));
        }
        auto purge = Statement::prepare(connection,
                                        "DELETE FROM tag WHERE NOT EXISTS "
                                        "(SELECT 1 FROM bookmark_tag WHERE tag_id = tag.id)");
        if (!purge) {
            return std::unexpected(std::move(purge.error()));
        }
        return RowWriter(std::move(*update), std::move(*unlink), std::move(*add_tag),
                         std::move(*link), std::move(*purge));
    }

    // Stores `updated` for bookmark `id`, whose stored values are `stored`.
    std::expected<void, Error> write(core::BookmarkId id, const core::Bookmark& stored,
                                     const core::Bookmark& updated, std::int64_t stored_time) {
        if (auto done = run(update_, updated.url, updated.title, updated.description,
                            updated.flags.bits(), stored_time, id.value());
            !done) {
            return done;
        }
        for (const std::string& name : stored.tags) {
            if (!updated.tags.contains(name)) {
                if (auto done = run(unlink_, id.value(), name); !done) {
                    return done;
                }
            }
        }
        for (const std::string& name : updated.tags) {
            if (!stored.tags.contains(name)) {
                if (auto done = run(add_tag_, name); !done) {
                    return done;
                }
                if (auto done = run(link_, id.value(), name); !done) {
                    return done;
                }
            }
        }
        return {};
    }

    // Deletes the tags that no bookmark links (docs/database-schema.md section 3).
    std::expected<void, Error> delete_unused_tags() { return run(purge_); }

private:
    // The five statements differ by their SQL only; a type for each would add nothing.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    RowWriter(Statement update, Statement unlink, Statement add_tag, Statement link,
              Statement purge)
        : update_(std::move(update)),
          unlink_(std::move(unlink)),
          add_tag_(std::move(add_tag)),
          link_(std::move(link)),
          purge_(std::move(purge)) {}

    Statement update_;
    Statement unlink_;
    Statement add_tag_;
    Statement link_;
    Statement purge_;
};

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
            .bookmark_id = holder,
            .range = std::nullopt});
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

std::expected<void, Error> update_bookmarks(Database& database, const core::IdSelection& selection,
                                            const core::BookmarkChanges& changes,
                                            std::chrono::sys_seconds now) {
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection, /*immediate=*/true);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    const std::vector<core::IdRange> ranges = core::covered_ranges(selection);

    if (auto found = check_targets_exist(connection, selection); !found) {
        return std::unexpected(std::move(found.error()));
    }
    if (changes.url.has_value()) {
        auto holder = find_by_url(connection, *changes.url);
        if (!holder) {
            return std::unexpected(std::move(holder.error()));
        }
        const std::optional<core::BookmarkId> existing = *holder;
        // A bookmark that already has the URL is no conflict, but only when it is the one target.
        const bool is_only_target = !selection.all && core::named_id_count(selection) == 1 &&
                                    existing.has_value() && ranges.front().first == *existing;
        if (existing.has_value() && !is_only_target) {
            return std::unexpected(Error{.code = ErrorCode::kDuplicateUrl,
                                         .message = "the URL is already stored as bookmark " +
                                                    std::to_string(existing->value()),
                                         .bookmark_id = existing,
                                         .range = std::nullopt});
        }
    }

    auto writer = RowWriter::prepare(connection);
    if (!writer) {
        return std::unexpected(std::move(writer.error()));
    }
    auto next = Statement::prepare(connection,
                                   "SELECT id, url, title, description, flags FROM bookmark "
                                   "WHERE id >= ? AND id <= ? ORDER BY id LIMIT 1");
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    auto select_tags = Statement::prepare(connection, kSelectTags);
    if (!select_tags) {
        return std::unexpected(std::move(select_tags.error()));
    }
    const std::int64_t stored_time = now.time_since_epoch().count();  // Unix seconds
    for (const core::IdRange& range : ranges) {
        std::int64_t cursor = range.first.value();
        for (;;) {
            // One lookup per row, never a SELECT left open while rows are written: SQLite leaves
            // undefined whether a pending SELECT sees the changes.
            if (auto bound = next->bind(1, cursor); !bound) {
                return std::unexpected(std::move(bound.error()));
            }
            if (auto bound = next->bind(2, range.last.value()); !bound) {
                return std::unexpected(std::move(bound.error()));
            }
            auto row = next->step();
            if (!row) {
                return std::unexpected(std::move(row.error()));
            }
            if (!*row) {
                std::ignore = next->reset();
                break;
            }
            const core::BookmarkId id(next->column_int(0));
            core::Bookmark stored{.url = next->column_text(1),
                                  .title = next->column_text(2),
                                  .description = next->column_text(3),
                                  .tags = {},
                                  .flags = core::FlagSet(next->column_int(4))};
            std::ignore = next->reset();
            auto tags = read_tags(*select_tags, id);
            if (!tags) {
                return std::unexpected(std::move(tags.error()));
            }
            stored.tags = std::move(*tags);
            const core::Bookmark updated = core::apply_changes(stored, changes);
            if (updated != stored) {
                if (auto written = writer->write(id, stored, updated, stored_time); !written) {
                    return std::unexpected(std::move(written.error()));
                }
            }
            if (id == range.last) {
                break;  // `id + 1` would overflow at the largest ID
            }
            cursor = id.value() + 1;
        }
    }

    if (auto cleaned = writer->delete_unused_tags(); !cleaned) {
        return std::unexpected(std::move(cleaned.error()));
    }
    return transaction->commit();
}

}  // namespace bucu::db
