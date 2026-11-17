#include "cli/change.hpp"

#include <ostream>
#include <string>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

namespace {

// Prints the stored record of each selected bookmark, with one empty line between records. Stops
// early, with the stream bad, when writing fails.
std::expected<void, db::Error> print_records(db::Database& database,
                                             const core::IdSelection& selection,
                                             std::ostream& out) {
    bool first = true;
    return db::for_each_bookmark(database, selection,
                                 [&](core::BookmarkId id, const core::Bookmark& bookmark) {
                                     if (!first) {
                                         out << '\n';
                                     }
                                     first = false;
                                     out << format_record(id, bookmark);
                                     return static_cast<bool>(out);
                                 });
}

}  // namespace

int run_change(const ChangeCommand& command, const Globals& globals, const Context& context,
               std::ostream& out, std::ostream& err) {
    const auto selection = core::parse_id_selection(command.id_arguments);
    if (!selection) {
        err << invalid_id_message(selection.error().argument) << '\n';
        return kExitUsage;
    }
    const auto changes = core::make_changes(command.request);
    if (!changes) {
        err << validation_error_message(changes.error()) << '\n';
        return kExitInvalid;
    }

    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    if (auto done = db::update_bookmarks(*database, *selection, *changes, context.now); !done) {
        err << sql_error_message("update the bookmarks", done.error()) << '\n';
        return kExitFailure;
    }
    if (auto printed = print_records(*database, *selection, out); !printed) {
        err << sql_error_message("read the bookmarks", printed.error()) << '\n';
        return kExitFailure;
    }
    return kExitOk;
}

}  // namespace bucu::cli
