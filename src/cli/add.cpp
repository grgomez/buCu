#include <ostream>
#include <string>

#include "bucu/core/bookmark.hpp"
#include "bucu/db/bookmarks.hpp"
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

int run_add(const AddArgs& args, const Globals& globals, const Context& context, std::ostream& out,
            std::ostream& err) {
    auto bookmark = core::make_bookmark(args.url, args.title, args.description, tag_text(args));
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
    if (!id && id.error().code == db::ErrorCode::kDuplicateUrl) {
        err << duplicate_url_message(bookmark->url, id.error().bookmark_id.value_or(0)) << '\n';
        return kExitConflict;
    }
    if (!id) {
        err << sql_error_message("store the bookmark", id.error()) << '\n';
        return kExitFailure;
    }
    out << format_record(*id, *bookmark);
    return kExitOk;
}

}  // namespace bucu::cli
