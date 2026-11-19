#include "cli/change.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "bucu/core/bookmark.hpp"
#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/id_selection.hpp"
#include "bucu/db/bookmarks.hpp"
#include "bucu/platform/terminal.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"
#include "cli/record.hpp"

namespace bucu::cli {

namespace {

// Prints the stored record of each selected bookmark, with one empty line between records. Stops
// early, with the stream bad, when writing fails.
std::expected<void, db::Error> print_records(db::Database& database,
                                             const core::IdSelection& selection,
                                             std::ostream& out) {
    bool first = true;
    return db::for_each_bookmark(database, selection,
                                 [&](core::BookmarkId id, const core::Bookmark& bookmark) {
                                     if (!first) {
                                         out << '\n';
                                     }
                                     first = false;
                                     out << format_record(id, bookmark);
                                     return static_cast<bool>(out);
                                 });
}

// Whether `answer` is a yes: y or yes in any case, ignoring spaces around it. Only ASCII is
// folded, so the answer does not depend on the locale.
bool is_yes(std::string_view answer) {
    constexpr std::string_view kSpace = " \t\n\r\f\v";
    const std::size_t first = answer.find_first_not_of(kSpace);
    if (first == std::string_view::npos) {
        return false;
    }
    answer = answer.substr(first, answer.find_last_not_of(kSpace) - first + 1);
    std::string lowered(answer);
    for (char& c : lowered) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return lowered == "y" || lowered == "yes";
}

// The checks and the question before a change to every bookmark (product section 5). Returns the
// exit code when the run ends here, or no value when the change goes on.
std::optional<int> confirm_all(db::Database& database, bool yes, const Context& context) {
    std::ostream& out = context.out;
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
    if (yes) {
        return std::nullopt;
    }
    if (!platform::is_interactive(context.terminals)) {
        err << usage_error_message(
                   "changing all bookmarks needs confirmation; add --yes to confirm")
            << '\n';
        return kExitUsage;
    }
    // The prompt is the one line that ends without LF: the user's Enter ends it.
    out << "Update all " << *count << " bookmarks? [y/N] " << std::flush;
    std::string answer;
    if (!std::getline(context.input.get(), answer)) {
        // End of input: no Enter ended the prompt line.
        out << '\n';
    } else if (is_yes(answer)) {
        return std::nullopt;
    }
    out << "Cancelled.\n";
    return kExitOk;
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
