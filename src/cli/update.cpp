#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "bucu/core/changes.hpp"
#include "cli/change.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"

namespace bucu::cli {

namespace {

// The --tag values as one tag input: every value is split on commas the same way, so joining them
// with commas keeps the union (product section 4, Tags).
std::string joined_tags(const std::vector<std::string>& values) {
    std::string text;
    for (const std::string& value : values) {
        text.append(text.empty() ? "" : ",");
        text.append(value);
    }
    return text;
}

}  // namespace

int run_update(const UpdateArgs& args, const Globals& globals, const Context& context,
               std::ostream& out, std::ostream& err) {
    const bool has_field = args.url || args.title || args.description || args.tags ||
                           args.immutable || args.no_immutable;
    if (!has_field) {
        err << usage_error_message(
                   "update needs at least one of --url, --title, --description, --tag, "
                   "--immutable, or --no-immutable")
            << '\n';
        return kExitUsage;
    }
    if (args.immutable && args.no_immutable) {
        err << usage_error_message("--immutable and --no-immutable cannot be used together")
            << '\n';
        return kExitUsage;
    }
    ChangeCommand command{.id_arguments = args.ids, .request = {}, .yes = args.yes};
    command.request.url = args.url;
    command.request.title = args.title;
    command.request.description = args.description;
    if (args.tags.has_value()) {
        command.request.tags = core::TagChange{.operation = core::TagOperation::kReplace,
                                               .text = joined_tags(*args.tags)};
    }
    if (args.immutable) {
        command.request.flag = core::FlagChange::kSet;
    }
    if (args.no_immutable) {
        command.request.flag = core::FlagChange::kClear;
    }
    return run_change(command, globals, context, out, err);
}

}  // namespace bucu::cli
