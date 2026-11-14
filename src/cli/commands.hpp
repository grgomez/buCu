#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "bucu/db/database.hpp"
#include "cli/run.hpp"

namespace bucu::cli {

// The global options that a command needs.
struct Globals {
    std::optional<std::string> db;  // --db
};

// The arguments of `add`, as CLI11 read them.
struct AddArgs {
    std::string url;
    std::vector<std::string> tags;         // the positional tags
    std::vector<std::string> tag_options;  // each --tag value
    std::string title;
    std::string description;
    bool immutable = false;
};

// Opens the database this run uses (product section 3), creating it when missing. On failure
// prints one error line to `err` and returns no value.
std::optional<db::Database> open_database(const Globals& globals, const Context& context,
                                          std::ostream& err);

// Runs `add` and returns the exit code.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int run_add(const AddArgs& args, const Globals& globals, const Context& context, std::ostream& out,
            std::ostream& err);

// The `db` subcommands. Each returns the exit code.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int run_db_path(const Globals& globals, const Context& context, std::ostream& out,
                std::ostream& err);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int run_db_version(const Globals& globals, const Context& context, std::ostream& out,
                   std::ostream& err);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int run_db_check(const Globals& globals, const Context& context, std::ostream& out,
                 std::ostream& err);

}  // namespace bucu::cli
