#include <optional>
#include <ostream>

#include "bucu/core/id_selection.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

int run_list(const ListArgs& args, const Globals& globals, const Context& context) {
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
    if (auto printed = print_records(*database, *selection, out); !printed) {
        const db::Error& error = printed.error();
        if (error.code == db::ErrorCode::kNotFound) {
            err << not_found_message(error) << '\n';
            return kExitNotFound;
        }
        err << sql_error_message("read the bookmarks", error) << '\n';
        return kExitFailure;
    }
    return kExitOk;
}

}  // namespace bucu::cli
