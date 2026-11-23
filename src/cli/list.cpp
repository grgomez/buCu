#include <cstdint>
#include <optional>
#include <ostream>

#include "bucu/core/id_selection.hpp"
#include "bucu/platform/terminal.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

int run_list(const ListArgs& args, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    // The usage errors come in a fixed order: --last with IDs, a bad --last value, a bad ID.
    if (args.last.has_value() && !args.ids.empty()) {
        err << usage_error_message("--last cannot be used with IDs") << '\n';
        return kExitUsage;
    }
    std::optional<std::int64_t> last_count;
    if (args.last.has_value()) {
        // The digit rule of an ID: a list or a range, even one that names a single ID, is refused.
        const auto count = core::parse_single_id(*args.last);
        if (!count) {
            err << invalid_last_message(*args.last) << '\n';
            return kExitUsage;
        }
        last_count = count->value();
    }
    const auto selection = core::parse_id_selection(args.ids);
    if (!selection) {
        err << invalid_id_message(selection.error().argument) << '\n';
        return kExitUsage;
    }
    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    const bool colour =
        platform::use_color(globals.color, context.terminals.output, context.environment.get());
    const auto printed = last_count ? print_last_records(*database, *last_count, out, colour)
                                    : print_records(*database, *selection, out, colour);
    if (!printed) {
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
