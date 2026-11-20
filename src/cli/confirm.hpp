#pragma once

#include <optional>
#include <string_view>

#include "cli/run.hpp"

namespace bucu::cli {

// The first step of a confirmed command (docs/product.md section 5, IDs and ranges): in a run that
// is not interactive and without `--yes`, prints the usage error "`needs` needs confirmation; add
// --yes to confirm" and returns the exit code to end with; otherwise returns no value. `needs`
// reads like "deleting bookmarks".
std::optional<int> check_can_confirm(const Context& context, bool yes, std::string_view needs);

// What the user answered.
enum class Answer : bool {
    kProceed,
    kCancelled,
};

// The second step: with `--yes`, proceeds without asking. Otherwise prints `prompt` (it ends in a
// space, and no LF: the user's Enter ends the line), reads one line, and proceeds on y or yes in
// any case. Any other answer, or the end of input, prints `Cancelled.` and cancels.
Answer ask(const Context& context, bool yes, std::string_view prompt);

}  // namespace bucu::cli
