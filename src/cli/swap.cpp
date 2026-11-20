#include <ostream>
#include <string>
#include <vector>

#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

int run_swap(const SwapArgs& args, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    // The count comes first, then each argument left to right, then equal IDs.
    if (args.ids.size() != 2) {
        err << usage_error_message("swap takes two IDs") << '\n';
        return kExitUsage;
    }
    std::vector<core::BookmarkId> ids;
    for (const std::string& argument : args.ids) {
        const auto id = core::parse_single_id(argument);
        if (!id) {
            if (id.error() == core::SingleIdError::kNotAnIdItem) {
                err << invalid_id_message(argument) << '\n';
            } else {
                err << usage_error_message("swap takes two IDs") << '\n';
            }
            return kExitUsage;
        }
        ids.push_back(*id);
    }
    if (ids[0] == ids[1]) {
        err << usage_error_message("swap needs two different IDs") << '\n';
        return kExitUsage;
    }
    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    if (auto done = db::swap_bookmark_ids(*database, ids[0], ids[1]); !done) {
        const db::Error& error = done.error();
        if (error.code == db::ErrorCode::kNotFound) {
            err << not_found_message(error) << '\n';
            return kExitNotFound;
        }
        err << sql_error_message("swap the IDs", error) << '\n';
        return kExitFailure;
    }
    const auto printed = print_records(*database, core::select_two_ids(ids[0], ids[1]), out);
    if (!printed) {
        err << sql_error_message("read the bookmarks", printed.error()) << '\n';
        return kExitFailure;
    }
    return kExitOk;
}

}  // namespace bucu::cli
