#include <optional>
#include <ostream>
#include <string>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/db/bookmarks.hpp"
#include "bucu/platform/terminal.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

namespace {

// All tag input as one text: the positional tags joined with spaces, then each --tag value. Every
// part is split on commas the same way, so joining them with commas keeps the union (product
// section 4, Tags).
std::string tag_text(const AddArgs& args) {
    std::string text;
    for (const std::string& word : args.tags) {
        text.append(text.empty() ? "" : " ");
        text.append(word);
    }
    for (const std::string& option : args.tag_options) {
        text.append(",");
        text.append(option);
    }
    return text;
}

}  // namespace

int run_add(const AddArgs& args, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    auto bookmark = core::make_bookmark({.url = core::RequiredText{args.url},
                                         .title = core::RequiredText{args.title},
                                         .description = core::RequiredText{args.description},
                                         .tags = tag_text(args)});
    if (!bookmark) {
        err << validation_error_message(bookmark.error()) << '\n';
        return kExitInvalid;
    }
    if (args.immutable) {
        bookmark->flags = core::set_flag(bookmark->flags, core::kFlagImmutableTitle);
    }

    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    const auto id = db::insert_bookmark(*database, *bookmark, context.now);
    if (!id) {
        const std::optional<core::BookmarkId>& holder = id.error().bookmark_id;
        if (id.error().code == db::ErrorCode::kDuplicateUrl && holder.has_value()) {
            err << duplicate_url_message(bookmark->url, *holder) << '\n';
            return kExitConflict;
        }
        err << sql_error_message("store the bookmark", id.error()) << '\n';
        return kExitFailure;
    }
    const bool colour =
        platform::use_color(globals.color, context.terminals.output, context.environment.get());
    out << format_record(*id, *bookmark, colour);
    return kExitOk;
}

}  // namespace bucu::cli
