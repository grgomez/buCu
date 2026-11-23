#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "bucu/db/database.hpp"
#include "bucu/platform/terminal.hpp"
#include "cli/run.hpp"

namespace bucu::cli {

// The global options that a command needs.
struct Globals {
    std::optional<std::string> db;                           // --db
    platform::ColorMode color = platform::ColorMode::kAuto;  // --color
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

// The arguments of `list`: the positional ID arguments (none means every bookmark), and the value
// of --last when it was given.
struct ListArgs {
    std::vector<std::string> ids;
    std::optional<std::string> last;
};

// The arguments of `search`: the keyword arguments, in the order given, and the words after `--`.
struct SearchArgs {
    std::vector<std::string> keywords;
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

// The arguments of `swap`, as CLI11 read them: whatever IDs were given, checked by `run_swap`.
struct SwapArgs {
    std::vector<std::string> ids;
};

// The arguments of `reorder`, as CLI11 read them. `ids` is whatever positional words were given;
// `run_reorder` refuses any.
struct ReorderArgs {
    std::vector<std::string> ids;
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

// Runs `list` and returns the exit code.
int run_list(const ListArgs& args, const Globals& globals, const Context& context);

// Runs `search` and returns the exit code.
int run_search(const SearchArgs& args, const Globals& globals, const Context& context);

// Runs `tag` and returns the exit code.
int run_tag(const TagArgs& args, const Globals& globals, const Context& context);

// Runs `delete` and returns the exit code.
int run_delete(const DeleteArgs& args, const Globals& globals, const Context& context);

// Runs `swap` and returns the exit code.
int run_swap(const SwapArgs& args, const Globals& globals, const Context& context);

// Runs `reorder` and returns the exit code.
int run_reorder(const ReorderArgs& args, const Globals& globals, const Context& context);

// The `db` subcommands. Each returns the exit code.
int run_db_path(const Globals& globals, const Context& context);
int run_db_version(const Globals& globals, const Context& context);

int run_db_check(const Globals& globals, const Context& context);

int run_db_backup(const std::string& file, const Globals& globals, const Context& context);

}  // namespace bucu::cli
