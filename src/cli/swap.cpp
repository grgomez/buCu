#include <ostream>
#include <string>

#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "bucu/platform/terminal.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

namespace {

// Prints the usage error for the argument that is not one ID, and returns its exit code.
int report_bad_id(const std::string& argument, core::SingleIdError error, std::ostream& err) {
    if (error == core::SingleIdError::kNotAnIdItem) {
        err << invalid_id_message(argument) << '\n';
    } else {
        err << usage_error_message("swap takes two IDs") << '\n';
    }
    return kExitUsage;
}

}  // namespace

int run_swap(const SwapArgs& args, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    // The count comes first, then each argument left to right, then equal IDs.
    if (args.ids.size() != 2) {
        err << usage_error_message("swap takes two IDs") << '\n';
        return kExitUsage;
    }
    const auto first = core::parse_single_id(args.ids[0]);
    if (!first) {
        return report_bad_id(args.ids[0], first.error(), err);
    }
    const auto second = core::parse_single_id(args.ids[1]);
    if (!second) {
        return report_bad_id(args.ids[1], second.error(), err);
    }
    if (*first == *second) {
        err << usage_error_message("swap needs two different IDs") << '\n';
        return kExitUsage;
    }
    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    if (auto done = db::swap_bookmark_ids(*database, *first, *second); !done) {
        const db::Error& error = done.error();
        if (error.code == db::ErrorCode::kNotFound) {
            err << not_found_message(error) << '\n';
            return kExitNotFound;
        }
        err << sql_error_message("swap the IDs", error) << '\n';
        return kExitFailure;
    }
    const bool colour =
        platform::use_color(globals.color, context.terminals.output, context.environment.get());
    const auto printed =
        print_records(*database, core::select_two_ids(*first, *second), out, colour);
    if (!printed) {
        err << sql_error_message("read the bookmarks", printed.error()) << '\n';
        return kExitFailure;
    }
    return kExitOk;
}

}  // namespace bucu::cli
