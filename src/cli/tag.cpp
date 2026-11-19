#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bucu/core/changes.hpp"
#include "bucu/core/id_selection.hpp"
#include "cli/change.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"

namespace bucu::cli {

namespace {

constexpr std::string_view kOperations = "add, remove, or replace";

std::optional<core::TagOperation> operation_named(std::string_view word) {
    if (word == "add") {
        return core::TagOperation::kAdd;
    }
    if (word == "remove") {
        return core::TagOperation::kRemove;
    }
    if (word == "replace") {
        return core::TagOperation::kReplace;
    }
    return std::nullopt;
}

// The usage error for a word list with no operation word: it names the first word that is not an
// ID item, or says that none was given.
std::string missing_operation_message(const std::vector<std::string>& words) {
    for (const std::string& word : words) {
        if (!core::parse_id_selection({word})) {
            return not_an_operation_message(word);
        }
    }
    return usage_error_message("tag needs an operation: " + std::string(kOperations));
}

}  // namespace

int run_tag(const TagArgs& args, const Globals& globals, const Context& context) {
    std::ostream& err = context.err;
    // The first word that names an operation is the operation: the words before it are IDs, and
    // the words after it are tags, even when they look like operations or commands.
    auto operation_word = args.words.begin();
    std::optional<core::TagOperation> found;
    for (; operation_word != args.words.end() && !found.has_value(); ++operation_word) {
        found = operation_named(*operation_word);
    }
    if (!found.has_value()) {
        err << missing_operation_message(args.words) << '\n';
        return kExitUsage;
    }
    const core::TagOperation operation = *found;
    --operation_word;  // the loop stepped past the operation word
    const std::vector<std::string> tag_words(operation_word + 1, args.words.end());
    if (tag_words.empty() && operation != core::TagOperation::kReplace) {
        err << usage_error_message("tag " + *operation_word + " needs at least one tag") << '\n';
        return kExitUsage;
    }

    // The tag words are joined with single spaces and read as one tag input, as in `add`.
    std::string text;
    for (const std::string& word : tag_words) {
        text.append(text.empty() ? "" : " ");
        text.append(word);
    }
    ChangeCommand command{
        .id_arguments = std::vector<std::string>(args.words.begin(), operation_word),
        .request = {},
        .yes = args.yes};
    command.request.tags = core::TagChange{.operation = operation, .text = std::move(text)};
    return run_change(command, globals, context);
}

}  // namespace bucu::cli
