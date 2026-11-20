#include <ostream>
#include <string>

#include "bucu/db/bookmarks.hpp"
#include "cli/commands.hpp"
#include "cli/confirm.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"

namespace bucu::cli {

int run_reorder(const ReorderArgs& args, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    if (!args.ids.empty()) {
        err << usage_error_message("reorder takes no ID") << '\n';
        return kExitUsage;
    }
    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    const auto count = db::count_bookmarks(*database);
    if (!count) {
        err << sql_error_message("count the bookmarks", count.error()) << '\n';
        return kExitFailure;
    }
    if (*count == 0) {
        err << no_bookmarks_message() << '\n';
        return kExitNotFound;
    }
    const auto misplaced = db::count_misplaced_ids(*database);
    if (!misplaced) {
        err << sql_error_message("read the IDs", misplaced.error()) << '\n';
        return kExitFailure;
    }
    if (*misplaced == 0) {
        out << already_in_order_message(*count) << '\n';
        return kExitOk;
    }
    if (const auto code = check_can_confirm(context, args.yes, "renumbering bookmarks")) {
        return *code;
    }
    if (ask(context, args.yes, renumber_prompt(*misplaced)) == Answer::kCancelled) {
        return kExitOk;
    }
    const auto moved = db::renumber_bookmarks(*database);
    if (!moved) {
        err << sql_error_message("renumber the bookmarks", moved.error()) << '\n';
        return kExitFailure;
    }
    out << renumbered_message(*moved) << '\n';
    return kExitOk;
}

}  // namespace bucu::cli
