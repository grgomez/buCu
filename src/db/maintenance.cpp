#include "bucu/db/maintenance.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "db/handle.hpp"
#include "db/statement.hpp"

namespace bucu::db {

namespace {

constexpr int kBusyTimeoutMs = 5000;  // database-schema section 1
constexpr int kBusyRetryMs = 50;

struct Closer {
    void operator()(sqlite3* connection) const noexcept { sqlite3_close(connection); }
};

// Removes the new file and the files SQLite may have made next to it. Errors are ignored: the
// copy has already failed, and its error is the one to report.
void remove_partial_copy(const std::filesystem::path& destination) {
    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    for (const char* suffix : {"-journal", "-wal", "-shm"}) {
        std::filesystem::remove(destination.string() + suffix, ignored);
    }
}

std::string message_of(sqlite3* connection, int code) {
    const char* message = sqlite3_errmsg(connection);
    return message != nullptr ? message : sqlite3_errstr(code);
}

// Copies every page in one step, retrying while a writer holds a lock, for up to the busy timeout
// of database-schema section 1. Returns the SQLite result code of the last attempt.
int copy_pages(sqlite3_backup* copy) {
    int waited = 0;
    int code = sqlite3_backup_step(copy, -1);
    while ((code == SQLITE_BUSY || code == SQLITE_LOCKED) && waited < kBusyTimeoutMs) {
        sqlite3_sleep(kBusyRetryMs);
        waited += kBusyRetryMs;
        code = sqlite3_backup_step(copy, -1);
    }
    return code;
}

}  // namespace

std::expected<std::vector<std::string>, Error> integrity_check(Database& database) {
    auto statement = Statement::prepare(detail::handle(database), "PRAGMA integrity_check");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }

    std::vector<std::string> report;
    for (;;) {
        const auto row = statement->step();
        if (!row) {
            // A damaged file gives rows and then a failure; the user needs both (a problem the
            // check cannot even describe is still a problem).
            report.push_back(row.error().message);
            break;
        }
        if (!*row) {
            break;
        }
        report.push_back(statement->column_text(0));
    }

    if (report.size() == 1 && report.front() == "ok") {
        report.clear();
    }
    return report;
}

std::expected<void, Error> backup(Database& database, const std::filesystem::path& destination) {
    std::error_code status;
    if (std::filesystem::exists(destination, status)) {
        return std::unexpected(
            Error{ErrorCode::kDestinationExists, "the file already exists", std::nullopt});
    }
    if (status) {
        return std::unexpected(Error{ErrorCode::kCannotOpen, status.message(), std::nullopt});
    }
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent, status)) {
        return std::unexpected(Error{ErrorCode::kCannotOpen,
                                     "the directory " + parent.string() + " does not exist",
                                     std::nullopt});
    }

    const std::u8string utf8 = destination.u8string();
    const std::string name(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    sqlite3* raw = nullptr;
    const int opened =
        sqlite3_open_v2(name.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    std::unique_ptr<sqlite3, Closer> target(raw);
    // Closes the connection first, because an open file cannot be removed on Windows.
    const auto fail = [&](ErrorCode code, std::string message) {
        target.reset();
        remove_partial_copy(destination);
        return std::unexpected(Error{code, std::move(message), std::nullopt});
    };
    if (opened != SQLITE_OK) {
        return fail(ErrorCode::kCannotOpen, message_of(raw, opened));
    }
    sqlite3_busy_timeout(raw, kBusyTimeoutMs);

    sqlite3_backup* copy = sqlite3_backup_init(raw, "main", detail::handle(database), "main");
    if (copy == nullptr) {
        return fail(ErrorCode::kSql, message_of(raw, sqlite3_errcode(raw)));
    }
    const int stepped = copy_pages(copy);
    const int failure = stepped != SQLITE_DONE ? stepped : SQLITE_OK;
    std::string message = failure != SQLITE_OK ? message_of(raw, failure) : std::string();
    const int finished = sqlite3_backup_finish(copy);
    if (failure != SQLITE_OK) {
        return fail(ErrorCode::kSql, std::move(message));
    }
    if (finished != SQLITE_OK) {
        return fail(ErrorCode::kSql, message_of(raw, finished));
    }
    return {};
}

}  // namespace bucu::db
