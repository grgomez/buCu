#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "bucu/core/changes.hpp"
#include "cli/commands.hpp"
#include "cli/run.hpp"

namespace bucu::cli {

// What `update` and `tag` share: the ID arguments, the changes, and whether `--yes` was given.
struct ChangeCommand {
    std::vector<std::string> id_arguments;
    core::ChangeRequest request;
    bool yes = false;
};

// Runs a change on the bookmarks the ID arguments name, and prints the record of each one that was
// selected (docs/product.md section 5). Returns the exit code.
int run_change(const ChangeCommand& command, const Globals& globals, const Context& context);

}  // namespace bucu::cli
