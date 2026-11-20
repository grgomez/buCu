#include <ostream>

#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "cli/commands.hpp"
#include "cli/confirm.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

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
    if (ask(context, args.yes, delete_prompt(selection->all, *count)) == Answer::kCancelled) {
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
