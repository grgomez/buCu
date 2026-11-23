#include "bucu/core/search.hpp"

#include <cstdint>
#include <ostream>

#include "bucu/core/error.hpp"
#include "bucu/db/bookmarks.hpp"
#include "bucu/platform/terminal.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

int run_search(const SearchArgs& args, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    const auto query = core::parse_search_query(args.keywords, false, false);
    if (!query) {
        // No keyword left is a usage error; a keyword that is not valid UTF-8 is a validation
        // error.
        if (query.error().reason == core::Reason::kEmpty) {
            err << usage_error_message("search needs at least one keyword") << '\n';
            return kExitUsage;
        }
        err << validation_error_message(query.error()) << '\n';
        return kExitInvalid;
    }
    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    const bool colour =
        platform::use_color(globals.color, context.terminals.output, context.environment.get());
    std::int64_t printed = 0;
    const auto done = db::for_each_search_result(
        *database, *query, [&](core::BookmarkId id, const core::Bookmark& bookmark) {
            if (printed > 0) {
                out << '\n';
            }
            ++printed;
            out << format_record(id, bookmark, colour);
            return static_cast<bool>(out);
        });
    if (!done) {
        err << sql_error_message("search the bookmarks", done.error()) << '\n';
        return kExitFailure;
    }
    return kExitOk;
}

}  // namespace bucu::cli
