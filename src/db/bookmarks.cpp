#include "bucu/db/bookmarks.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

// The statements that read bookmarks with their tags, prepared once per command.
class RowReader {
public:
    static std::expected<RowReader, Error> prepare(sqlite3* connection) {
        return prepare_with(connection,
                            "SELECT id, url, title, description, flags FROM bookmark "
                            "WHERE id >= ? AND id <= ? ORDER BY id");
    }

    // A reader over `sql`, which selects id, url, title, description, and flags, in this order.
    static std::expected<RowReader, Error> prepare_with(sqlite3* connection, std::string_view sql) {
        auto select = Statement::prepare(connection, sql);
        if (!select) {
            return std::unexpected(std::move(select.error()));
        }
        auto select_tags = Statement::prepare(connection, kSelectTags);
        if (!select_tags) {
            return std::unexpected(std::move(select_tags.error()));
        }
        return RowReader(std::move(*select), std::move(*select_tags));
    }

    // Calls `visit` for each stored bookmark with an ID from `first` to `last`, in ascending
    // order, one row at a time. Returns `false` when `visit` stopped the walk.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the bounds are always first, then last
    std::expected<bool, Error> visit_range(std::int64_t first, std::int64_t last,
                                           const BookmarkVisitor& visit) {
        if (auto bound = select_.bind(1, first); !bound) {
            return std::unexpected(std::move(bound.error()));
        }
        if (auto bound = select_.bind(2, last); !bound) {
            return std::unexpected(std::move(bound.error()));
        }
        return visit_rows(visit);
    }

    // Binds `value` to the parameter of the statement, which has one, and visits its rows as
    // `visit_range` does.
    std::expected<bool, Error> visit_with_parameter(std::int64_t value,
                                                    const BookmarkVisitor& visit) {
        if (auto bound = select_.bind(1, value); !bound) {
            return std::unexpected(std::move(bound.error()));
        }
        return visit_rows(visit);
    }

private:
    std::expected<bool, Error> visit_rows(const BookmarkVisitor& visit) {
        for (;;) {
            auto row = select_.step();
            if (!row) {
                return std::unexpected(std::move(row.error()));
            }
            if (!*row) {
                break;
            }
            const core::BookmarkId id(select_.column_int(0));
            auto tags = read_tags(select_tags_, id);
            if (!tags) {
                return std::unexpected(std::move(tags.error()));
            }
            const core::Bookmark bookmark{.url = select_.column_text(1),
                                          .title = select_.column_text(2),
                                          .description = select_.column_text(3),
                                          .tags = std::move(*tags),
                                          .flags = core::FlagSet(select_.column_int(4))};
            if (!visit(id, bookmark)) {
                return false;
            }
        }
        std::ignore = select_.reset();
        return true;
    }

    // The two statements differ by their SQL only; a type for each would add nothing.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    RowReader(Statement select, Statement select_tags)
        : select_(std::move(select)), select_tags_(std::move(select_tags)) {}

    Statement select_;
    Statement select_tags_;
};

// The separator of the tags in the text that `bucu_keyword_score` receives. No tag holds a control
// character (docs/product.md section 4, Tags), so it cannot appear inside a tag.
constexpr char kTagSeparator = '\x1F';

constexpr const char* kKeywordScoreFunction = "bucu_keyword_score";
constexpr int kKeywordScoreArguments = 4;  // url, title, description, tags

// The SELECT of the search passes: the bookmarks whose score is the one parameter, in ID order.
// SQLite walks the table in ID order, so no sorter holds the rows.
constexpr const char* kSelectByScore =
    "SELECT id, url, title, description, flags FROM bookmark "
    "WHERE bucu_keyword_score(url, title, description, "
    "(SELECT group_concat(tag.name, char(31)) FROM bookmark_tag "
    "JOIN tag ON tag.id = bookmark_tag.tag_id WHERE bookmark_tag.bookmark_id = bookmark.id)) = ? "
    "ORDER BY id";

// The text of SQL value `value` as a view, valid until the next call on the same value. A NULL is
// empty.
std::string_view value_text(sqlite3_value* value) {
    const unsigned char* const text = sqlite3_value_text(value);
    if (text == nullptr) {
        return {};
    }
    // SQLite gives text as unsigned char; std::string_view takes char.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return {reinterpret_cast<const char*>(text),
            static_cast<std::size_t>(sqlite3_value_bytes(value))};
}

// The SQL function bucu_keyword_score(url, title, description, tags): how many keywords of the
// SearchQuery in the user data match the bookmark (core::count_matching_keywords). `tags` holds
// each tag, separated by kTagSeparator. BuCu code does not catch exceptions outside `cli`, so a
// failed allocation here ends the process, as it does anywhere else in BuCu.
void keyword_score(sqlite3_context* context, int argument_count, sqlite3_value** arguments) {
    if (argument_count != kKeywordScoreArguments) {
        sqlite3_result_error_code(context, SQLITE_MISUSE);
        return;
    }
    const auto* const query = static_cast<const core::SearchQuery*>(sqlite3_user_data(context));
    std::vector<std::string_view> fields;
    // arguments[0] to [2] are the URL, title, and description.
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    fields.push_back(value_text(arguments[0]));
    fields.push_back(value_text(arguments[1]));
    fields.push_back(value_text(arguments[2]));
    std::string_view tags = value_text(arguments[3]);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    while (!tags.empty()) {
        const std::size_t end = tags.find(kTagSeparator);
        fields.push_back(tags.substr(0, end));
        tags = end == std::string_view::npos ? std::string_view{} : tags.substr(end + 1);
    }
    sqlite3_result_int64(context,
                         static_cast<sqlite3_int64>(core::count_matching_keywords(*query, fields)));
}

// Registers bucu_keyword_score for `query` on a connection, and removes it when destroyed, so the
// connection never keeps a pointer to a query that is gone.
class KeywordScoreFunction {
public:
    static std::expected<KeywordScoreFunction, Error> add(sqlite3* connection,
                                                          const core::SearchQuery& query) {
        // The function reads `query` through the user data, which SQLite takes as a non-const
        // pointer; keyword_score only reads it.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        void* const data = const_cast<core::SearchQuery*>(&query);
        const int code =
            sqlite3_create_function_v2(connection, kKeywordScoreFunction, kKeywordScoreArguments,
                                       SQLITE_UTF8, data, keyword_score, nullptr, nullptr, nullptr);
        if (code != SQLITE_OK) {
            return std::unexpected(Error{.code = ErrorCode::kSql,
                                         .message = sqlite3_errmsg(connection),
                                         .bookmark_id = std::nullopt,
                                         .range = std::nullopt});
        }
        return KeywordScoreFunction(connection);
    }

    KeywordScoreFunction(KeywordScoreFunction&& other) noexcept
        : connection_(std::exchange(other.connection_, nullptr)) {}
    KeywordScoreFunction& operator=(KeywordScoreFunction&&) = delete;
    KeywordScoreFunction(const KeywordScoreFunction&) = delete;
    KeywordScoreFunction& operator=(const KeywordScoreFunction&) = delete;

    ~KeywordScoreFunction() {
        if (connection_ != nullptr) {
            sqlite3_create_function_v2(connection_, kKeywordScoreFunction, kKeywordScoreArguments,
                                       SQLITE_UTF8, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
    }

private:
    explicit KeywordScoreFunction(sqlite3* connection) : connection_(connection) {}

    sqlite3* connection_;
};

// Makes the foreign key checks wait for the commit of the open transaction, so `bookmark.id` and
// the `bookmark_tag` rows that reference it (no ON UPDATE CASCADE) can change one after the other.
// SQLite resets the pragma when the transaction ends.
std::expected<void, Error> defer_foreign_keys(sqlite3* connection) {
    auto defer = Statement::prepare(connection, "PRAGMA defer_foreign_keys = ON");
    if (!defer) {
        return std::unexpected(std::move(defer.error()));
    }
    return run(*defer);
}

// Runs each statement of `statements` once, with no parameter, in order.
std::expected<void, Error> run_each(sqlite3* connection,
                                    std::initializer_list<const char*> statements) {
    for (const char* sql : statements) {
        auto statement = Statement::prepare(connection, sql);
        if (!statement) {
            return std::unexpected(std::move(statement.error()));
        }
        if (auto done = run(*statement); !done) {
            return done;
        }
    }
    return {};
}

// Deletes the tags that no bookmark links (docs/database-schema.md section 3).
constexpr const char* kPurgeUnusedTags =
    "DELETE FROM tag WHERE NOT EXISTS (SELECT 1 FROM bookmark_tag WHERE tag_id = tag.id)";

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
        auto purge = Statement::prepare(connection, kPurgeUnusedTags);
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
    // Named IDs must be stored, checked before the first visit; every bookmark may be none.
    if (!selection.all) {
        if (auto found = check_targets_exist(connection, selection); !found) {
            return std::unexpected(std::move(found.error()));
        }
    }
    auto reader = RowReader::prepare(connection);
    if (!reader) {
        return std::unexpected(std::move(reader.error()));
    }
    for (const core::IdRange& range : core::covered_ranges(selection)) {
        auto more = reader->visit_range(range.first.value(), range.last.value(), visit);
        if (!more) {
            return std::unexpected(std::move(more.error()));
        }
        if (!*more) {
            break;
        }
    }
    return {};
}

std::expected<void, Error> for_each_search_result(Database& database,
                                                  const core::SearchQuery& query,
                                                  const BookmarkVisitor& visit) {
    sqlite3* const connection = detail::handle(database);
    // One read transaction, so every pass reads the same snapshot.
    auto transaction = Transaction::begin(connection);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    // Declared before the reader, so the statements are finalized before the function goes.
    auto function = KeywordScoreFunction::add(connection, query);
    if (!function) {
        return std::unexpected(std::move(function.error()));
    }
    auto reader = RowReader::prepare_with(connection, kSelectByScore);
    if (!reader) {
        return std::unexpected(std::move(reader.error()));
    }
    const auto keyword_count = static_cast<std::int64_t>(query.keywords.size());
    const std::int64_t lowest_score = query.all ? keyword_count : 1;
    for (std::int64_t score = keyword_count; score >= lowest_score; --score) {
        auto more = reader->visit_with_parameter(score, visit);
        if (!more) {
            return std::unexpected(std::move(more.error()));
        }
        if (!*more) {
            break;
        }
    }
    return {};
}

std::expected<void, Error> for_each_last_bookmark(Database& database, std::int64_t count,
                                                  const BookmarkVisitor& visit) {
    if (count < 1) {
        return {};
    }
    sqlite3* const connection = detail::handle(database);
    // One read transaction for both statements, so the lowest ID and the walk share a snapshot.
    auto transaction = Transaction::begin(connection);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    auto lowest =
        Statement::prepare(connection, "SELECT id FROM bookmark ORDER BY id DESC LIMIT 1 OFFSET ?");
    if (!lowest) {
        return std::unexpected(std::move(lowest.error()));
    }
    if (auto bound = lowest->bind(1, count - 1); !bound) {
        return std::unexpected(std::move(bound.error()));
    }
    auto row = lowest->step();
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    // No row at the offset: fewer than `count` bookmarks are stored, so every one is wanted.
    const std::int64_t low = *row ? lowest->column_int(0) : 0;
    auto reader = RowReader::prepare(connection);
    if (!reader) {
        return std::unexpected(std::move(reader.error()));
    }
    auto done = reader->visit_range(low, std::numeric_limits<std::int64_t>::max(), visit);
    if (!done) {
        return std::unexpected(std::move(done.error()));
    }
    return {};
}

std::expected<void, Error> check_bookmarks_exist(Database& database,
                                                 const core::IdSelection& selection) {
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    return check_targets_exist(connection, selection);
}

std::expected<std::int64_t, Error> delete_bookmarks(Database& database,
                                                    const core::IdSelection& selection) {
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection, /*immediate=*/true);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    if (auto found = check_targets_exist(connection, selection); !found) {
        return std::unexpected(std::move(found.error()));
    }
    // One DELETE per range, so a range such as 1-9999999999 is never expanded. The cascade from
    // bookmark_tag removes the links.
    auto remove = Statement::prepare(connection, "DELETE FROM bookmark WHERE id >= ? AND id <= ?");
    if (!remove) {
        return std::unexpected(std::move(remove.error()));
    }
    std::int64_t deleted = 0;
    for (const core::IdRange& range : core::covered_ranges(selection)) {
        if (auto done = run(*remove, range.first.value(), range.last.value()); !done) {
            return std::unexpected(std::move(done.error()));
        }
        deleted += sqlite3_changes64(connection);
    }
    auto purge = Statement::prepare(connection, kPurgeUnusedTags);
    if (!purge) {
        return std::unexpected(std::move(purge.error()));
    }
    if (auto done = run(*purge); !done) {
        return std::unexpected(std::move(done.error()));
    }
    if (auto committed = transaction->commit(); !committed) {
        return std::unexpected(std::move(committed.error()));
    }
    return deleted;
}

std::expected<void, Error> swap_bookmark_ids(Database& database, core::BookmarkId a,
                                             core::BookmarkId b) {
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection, /*immediate=*/true);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    // The items are in ascending order, so the first failure is the lower ID.
    if (auto found = check_targets_exist(connection, core::select_two_ids(a, b)); !found) {
        return std::unexpected(std::move(found.error()));
    }

    if (auto deferred = defer_foreign_keys(connection); !deferred) {
        return std::unexpected(std::move(deferred.error()));
    }
    // Through negative IDs, so the two never share an ID: a -> -a, b -> -b, then -a -> b, -b -> a.
    for (const char* sql : {"UPDATE bookmark SET id = -id WHERE id IN (?1, ?2)",
                            "UPDATE bookmark_tag SET bookmark_id = -bookmark_id "
                            "WHERE bookmark_id IN (?1, ?2)",
                            "UPDATE bookmark SET id = CASE WHEN id = -?1 THEN ?2 ELSE ?1 END "
                            "WHERE id IN (-?1, -?2)",
                            "UPDATE bookmark_tag SET bookmark_id = "
                            "CASE WHEN bookmark_id = -?1 THEN ?2 ELSE ?1 END "
                            "WHERE bookmark_id IN (-?1, -?2)"}) {
        auto statement = Statement::prepare(connection, sql);
        if (!statement) {
            return std::unexpected(std::move(statement.error()));
        }
        if (auto done = run(*statement, a.value(), b.value()); !done) {
            return std::unexpected(std::move(done.error()));
        }
    }
    return transaction->commit();
}

std::expected<std::int64_t, Error> count_misplaced_ids(Database& database) {
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    auto statement = Statement::prepare(
        connection,
        "SELECT count(*) FROM (SELECT id, row_number() OVER (ORDER BY id) AS place FROM bookmark) "
        "WHERE id <> place");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }
    auto row = statement->step();
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    return statement->column_int(0);
}

std::expected<std::int64_t, Error> renumber_bookmarks(Database& database) {
    sqlite3* const connection = detail::handle(database);
    auto transaction = Transaction::begin(connection, /*immediate=*/true);
    if (!transaction) {
        return std::unexpected(std::move(transaction.error()));
    }
    if (auto deferred = defer_foreign_keys(connection); !deferred) {
        return std::unexpected(std::move(deferred.error()));
    }
    // The map from old to new ID lives in SQLite, so no result set is held here. Only the
    // bookmarks that move are in it.
    if (auto done = run_each(
            connection,
            {"CREATE TEMP TABLE id_map AS SELECT id AS old_id, row_number() OVER (ORDER BY id) AS "
             "new_id FROM bookmark",
             "DELETE FROM id_map WHERE old_id = new_id"});
        !done) {
        return std::unexpected(std::move(done.error()));
    }
    auto count = Statement::prepare(connection, "SELECT count(*) FROM id_map");
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    auto row = count->step();
    if (!row) {
        return std::unexpected(std::move(row.error()));
    }
    const std::int64_t moved = count->column_int(0);
    std::ignore = count->reset();
    // Through negative IDs, so a new ID never meets an old one that has not moved yet.
    if (auto done = run_each(
            connection,
            {"UPDATE bookmark SET id = -id WHERE id IN (SELECT old_id FROM id_map)",
             "UPDATE bookmark_tag SET bookmark_id = -bookmark_id "
             "WHERE bookmark_id IN (SELECT old_id FROM id_map)",
             "UPDATE bookmark SET id = (SELECT new_id FROM id_map WHERE old_id = -bookmark.id) "
             "WHERE id < 0",
             "UPDATE bookmark_tag SET bookmark_id = "
             "(SELECT new_id FROM id_map WHERE old_id = -bookmark_tag.bookmark_id) "
             "WHERE bookmark_id < 0",
             "DROP TABLE temp.id_map"});
        !done) {
        return std::unexpected(std::move(done.error()));
    }
    if (auto committed = transaction->commit(); !committed) {
        return std::unexpected(std::move(committed.error()));
    }
    return moved;
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
