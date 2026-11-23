#include "cli/change.hpp"

#include <optional>
#include <ostream>
#include <string>

#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "cli/confirm.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

namespace {

// The checks and the question before a change to every bookmark (product section 5). Returns the
// exit code when the run ends here, or no value when the change goes on.
std::optional<int> confirm_all(db::Database& database, bool yes, const Context& context) {
    std::ostream& err = context.err;
    const auto count = db::count_bookmarks(database);
    if (!count) {
        err << sql_error_message("count the bookmarks", count.error()) << '\n';
        return kExitFailure;
    }
    if (*count == 0) {
        err << not_found_message(db::Error{.code = db::ErrorCode::kNotFound,
                                           .message = "no bookmark is stored",
                                           .bookmark_id = std::nullopt,
                                           .range = std::nullopt})
            << '\n';
        return kExitNotFound;
    }
    if (const auto code = check_can_confirm(context, yes, "changing all bookmarks")) {
        return code;
    }
    if (ask(context, yes, "Update all " + std::to_string(*count) + " bookmarks? [y/N] ") ==
        Answer::kCancelled) {
        return kExitOk;
    }
    return std::nullopt;
}

}  // namespace

int run_change(const ChangeCommand& command, const Globals& globals, const Context& context) {
    std::ostream& out = context.out;
    std::ostream& err = context.err;
    const auto selection = core::parse_id_selection(command.id_arguments);
    if (!selection) {
        err << invalid_id_message(selection.error().argument) << '\n';
        return kExitUsage;
    }
    // Two bookmarks cannot share a URL, so --url needs exactly one named ID (product section 5).
    if (command.request.url.has_value() && core::named_id_count(*selection) != 1) {
        err << usage_error_message(
                   "--url needs exactly one bookmark ID, because two bookmarks cannot share a URL")
            << '\n';
        return kExitUsage;
    }
    const auto changes = core::make_changes(command.request);
    if (!changes) {
        err << validation_error_message(changes.error()) << '\n';
        return kExitInvalid;
    }

    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    if (selection->all) {
        if (const auto code = confirm_all(*database, command.yes, context)) {
            return *code;
        }
    }
    if (auto done = db::update_bookmarks(*database, *selection, *changes, context.now); !done) {
        const db::Error& error = done.error();
        if (error.code == db::ErrorCode::kNotFound) {
            err << not_found_message(error) << '\n';
            return kExitNotFound;
        }
        const std::optional<std::string>& new_url = changes->url;
        if (error.code == db::ErrorCode::kDuplicateUrl && error.bookmark_id.has_value() &&
            new_url.has_value()) {
            err << duplicate_url_message(*new_url, *error.bookmark_id) << '\n';
            return kExitConflict;
        }
        err << sql_error_message("update the bookmarks", error) << '\n';
        return kExitFailure;
    }
    if (auto printed = print_records(*database, *selection, out); !printed) {
        err << sql_error_message("read the bookmarks", printed.error()) << '\n';
        return kExitFailure;
    }
    return kExitOk;
}

}  // namespace bucu::cli
