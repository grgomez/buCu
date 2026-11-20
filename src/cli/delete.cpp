#include <cstdint>
#include <expected>
#include <ostream>
#include <string>
#include <utility>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "cli/commands.hpp"
#include "cli/confirm.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

namespace {

// Prints the record of each selected bookmark, with one empty line between records, and returns
// how many it printed. Stops early, with the stream bad, when writing fails.
std::expected<std::int64_t, db::Error> print_records(db::Database& database,
                                                     const core::IdSelection& selection,
                                                     std::ostream& out) {
    std::int64_t count = 0;
    auto done = db::for_each_bookmark(database, selection,
                                      [&](core::BookmarkId id, const core::Bookmark& bookmark) {
                                          if (count > 0) {
                                              out << '\n';
                                          }
                                          ++count;
                                          out << format_record(id, bookmark);
                                          return static_cast<bool>(out);
                                      });
    if (!done) {
        return std::unexpected(std::move(done.error()));
    }
    return count;
}

}  // namespace

int run_delete(const DeleteArgs& args, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    const auto selection = core::parse_id_selection(args.ids);
    if (!selection) {
        err << invalid_id_message(selection.error().argument) << '\n';
        return kExitUsage;
    }
    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    if (auto found = db::check_bookmarks_exist(*database, *selection); !found) {
        const db::Error& error = found.error();
        if (error.code == db::ErrorCode::kNotFound) {
            err << not_found_message(error) << '\n';
            return kExitNotFound;
        }
        err << sql_error_message("read the bookmarks", error) << '\n';
        return kExitFailure;
    }
    if (const auto code = check_can_confirm(context, args.yes, "deleting bookmarks")) {
        return *code;
    }
    // With no ID, the records are not shown: only the count is.
    const auto count =
        selection->all ? db::count_bookmarks(*database) : print_records(*database, *selection, out);
    if (!count) {
        err << sql_error_message("read the bookmarks", count.error()) << '\n';
        return kExitFailure;
    }
    if (!out) {
        return kExitFailure;  // nothing is deleted that could not be shown
    }
    if (!selection->all && *count > 0) {
        out << '\n';
    }
    const std::string prompt = std::string("Delete ") + (selection->all ? "all " : "") +
                               std::to_string(*count) + (*count == 1 ? " bookmark" : " bookmarks") +
                               "? [y/N] ";
    if (ask(context, args.yes, prompt) == Answer::kCancelled) {
        return kExitOk;
    }
    const auto deleted = db::delete_bookmarks(*database, *selection);
    if (!deleted) {
        const db::Error& error = deleted.error();
        if (error.code == db::ErrorCode::kNotFound) {
            err << not_found_message(error) << '\n';
            return kExitNotFound;
        }
        err << sql_error_message("delete the bookmarks", error) << '\n';
        return kExitFailure;
    }
    out << deleted_message(*deleted) << '\n';
    return kExitOk;
}

}  // namespace bucu::cli
