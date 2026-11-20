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

// The arguments of `update`, as CLI11 read them. An option that was not given is a std::nullopt,
// so an empty value is told apart from no value.
struct UpdateArgs {
    std::vector<std::string> ids;  // the positional ID arguments; none means every bookmark
    std::optional<std::string> url;
    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<std::vector<std::string>> tags;  // each --tag value, when --tag was given
    bool immutable = false;
    bool no_immutable = false;
    bool yes = false;
};

// The arguments of `tag`: the IDs, the operation word, and the tags, in the order given.
struct TagArgs {
    std::vector<std::string> words;
    bool yes = false;
};

// The arguments of `delete`, as CLI11 read them.
struct DeleteArgs {
    std::vector<std::string> ids;  // the positional ID arguments; none means every bookmark
    bool yes = false;
};

// Opens the database this run uses (product section 3), creating it when missing. On failure
// prints one error line to `err` and returns no value.
std::optional<db::Database> open_database(const Globals& globals, const Context& context,
                                          std::ostream& err);

// Runs `add` and returns the exit code.
int run_add(const AddArgs& args, const Globals& globals, const Context& context);

// Runs `update` and returns the exit code.
int run_update(const UpdateArgs& args, const Globals& globals, const Context& context);

// Runs `tag` and returns the exit code.
int run_tag(const TagArgs& args, const Globals& globals, const Context& context);

// Runs `delete` and returns the exit code.
int run_delete(const DeleteArgs& args, const Globals& globals, const Context& context);

// The `db` subcommands. Each returns the exit code.
int run_db_path(const Globals& globals, const Context& context);
int run_db_version(const Globals& globals, const Context& context);

int run_db_check(const Globals& globals, const Context& context);

int run_db_backup(const std::string& file, const Globals& globals, const Context& context);

}  // namespace bucu::cli
