#include <algorithm>
#include <expected>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

namespace {

// The selection of the two IDs, which differ.
core::IdSelection selection_of(core::BookmarkId a, core::BookmarkId b) {
    const auto single = [](core::BookmarkId id) {
        return core::IdItem{.kind = core::IdItemKind::kSingle,
                            .range = core::IdRange{.first = id, .last = id}};
    };
    return core::IdSelection{.all = false,
                             .items = {single(std::min(a, b)), single(std::max(a, b))}};
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
    bool first = true;
    const auto printed =
        db::for_each_bookmark(*database, selection_of(ids[0], ids[1]),
                              [&](core::BookmarkId id, const core::Bookmark& bookmark) {
                                  if (!first) {
                                      out << '\n';
                                  }
                                  first = false;
                                  out << format_record(id, bookmark);
                                  return static_cast<bool>(out);
                              });
    if (!printed) {
        err << sql_error_message("read the bookmarks", printed.error()) << '\n';
        return kExitFailure;
    }
    return kExitOk;
}

}  // namespace bucu::cli
