#include "cli/run.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "CLI11.hpp"
#include "bucu/db/database.hpp"
#include "bucu/platform/paths.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"

namespace bucu::cli {

namespace {

constexpr const char* kHelpText = R"(bucu - a command-line bookmark manager

Usage:
  bucu [--db <path>] [--color <when>] [--offline] <command> [args]
  bucu --version | -h | --help

Options (before the command):
  -h, --help          Print this help text and exit.
      --version       Print the version and exit.
      --db <path>     Use this database file.
      --color <when>  Colour output: auto, always, or never.
      --offline       Make no network request.

Commands:
  add <url> [tags...]
      Add a bookmark. Tags are comma-separated; a tag that starts with -
      goes after -- or in --tag=<tag>.
        --title <text>        Set the title.
        --description <text>  Set the description.
        --tag <tags>          Add tags; repeat the option for more.
        --immutable           Keep a later title fetch from replacing the title.
  db path               Print the database location.
  db version            Print the schema version.
  db check              Check the database for damage.
  db backup <file>      Write a consistent copy to a new file.

Exit codes:
  0  success
  1  a runtime failure
  2  a usage error
  3  a validation error: a value broke a bookmark rule
  4  a conflict: the URL is already bookmarked
)";

// Replaces CLI11's generated help with the fixed text above.
class HelpFormatter : public CLI::FormatterBase {
public:
    std::string make_help(const CLI::App*, std::string, CLI::AppFormatMode) const override {
        return kHelpText;
    }
};

// Returns `code`, or 1 with a message when writing to `out` failed (product section 6).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int finish(int code, std::ostream& out, std::ostream& err) {
    out.flush();
    if (!out) {
        err << "bucu: cannot write to standard output\n";
        return kExitFailure;
    }
    return code;
}

}  // namespace

std::optional<db::Database> open_database(const Globals& globals, const Context& context,
                                          std::ostream& err) {
    const auto location =
        platform::resolve_database_location(context.os, context.environment, globals.db);
    if (!location) {
        err << location_error_message(location.error()) << '\n';
        return std::nullopt;
    }
    const auto file = platform::prepare_database_location(*location);
    if (!file) {
        err << location_error_message(file.error()) << '\n';
        return std::nullopt;
    }
    auto database = db::Database::open(*file);
    if (!database) {
        err << open_error_message(platform::utf8_from_path(*file), database.error()) << '\n';
        return std::nullopt;
    }
    return std::move(*database);
}

namespace {

// Parses and runs `args`; `extra_tags` are more positional tags for `add`. A probe is silent: when
// its parse fails or the command is not `add`, it returns no value and nothing has run.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
std::optional<int> parse_and_run(const std::vector<std::string>& args,
                                 const std::vector<std::string>& extra_tags, bool probe,
                                 std::ostream& out, std::ostream& err, const Context& context) {
    // An empty name keeps CLI11 from prefixing its messages with the program name.
    CLI::App app{"", ""};
    app.formatter(std::make_shared<HelpFormatter>());
    app.set_version_flag("--version", "bucu " BUCU_VERSION);

    // The global options come before the command; CLI11 refuses them after it (product section 5).
    // --color and --offline are checked here and take effect with later issues.
    Globals globals;
    std::string color;
    bool offline = false;
    app.add_option("--db", globals.db)->expected(1);
    app.add_option("--color", color)->check(CLI::IsMember({"auto", "always", "never"}));
    app.add_flag("--offline", offline);

    AddArgs add_args;
    CLI::App* add = app.add_subcommand("add");
    add->add_option("url", add_args.url)->required();
    add->add_option("tags", add_args.tags);
    add->add_option("--title", add_args.title)->expected(1);
    add->add_option("--description", add_args.description)->expected(1);
    // One value per use, so the positional tags after it stay positional.
    add->add_option("--tag", add_args.tag_options)->allow_extra_args(false);
    add->add_flag("--immutable", add_args.immutable);

    CLI::App* db = app.add_subcommand("db");
    db->require_subcommand(1);
    CLI::App* db_path = db->add_subcommand("path");
    CLI::App* db_version = db->add_subcommand("version");
    CLI::App* db_check = db->add_subcommand("check");
    std::string backup_file;
    CLI::App* db_backup = db->add_subcommand("backup");
    db_backup->add_option("file", backup_file)->required();

    // CLI11 reads the arguments from the back of the vector.
    std::vector<std::string> reversed(args.rbegin(), args.rend());
    try {
        app.parse(reversed);
    } catch (const CLI::CallForHelp& help) {
        if (probe) {
            return std::nullopt;
        }
        return finish(app.exit(help, out, err), out, err);
    } catch (const CLI::CallForVersion& version) {
        if (probe) {
            return std::nullopt;
        }
        return finish(app.exit(version, out, err), out, err);
    } catch (const CLI::ParseError& error) {
        if (probe) {
            return std::nullopt;
        }
        std::string what = error.what();
        if (what.ends_with('.')) {
            what.pop_back();
        }
        err << "bucu: " << what << ". Run 'bucu --help' for usage.\n";
        return kExitUsage;
    }
    if (probe && !add->parsed()) {
        return std::nullopt;
    }
    add_args.tags.insert(add_args.tags.end(), extra_tags.begin(), extra_tags.end());
    if (add->parsed()) {
        return finish(run_add(add_args, globals, context, out, err), out, err);
    }
    if (db_path->parsed()) {
        return finish(run_db_path(globals, context, out, err), out, err);
    }
    if (db_version->parsed()) {
        return finish(run_db_version(globals, context, out, err), out, err);
    }
    if (db_check->parsed()) {
        return finish(run_db_check(globals, context, out, err), out, err);
    }
    if (db_backup->parsed()) {
        return finish(run_db_backup(backup_file, globals, context, out, err), out, err);
    }
    // Only global options were given. An unknown argument was already refused by the parser.
    err << "bucu: no command was given. Run 'bucu --help' for usage.\n";
    return kExitUsage;
}

}  // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err,
        const Context& context) {
    // A lone "--" has nothing after it to run, so it counts as a bare call.
    if (args.empty() || (args.size() == 1 && args[0] == "--")) {
        err << kHelpText;
        return kExitUsage;
    }

    // CLI11 ends `add` at "--" once a positional word was read, which would lose a tag such as -x
    // in `add <url> web -- -x` (product section 5). So when `add` parses without the words after
    // "--", those words are appended to its tags. Any other command, and any failure, is parsed
    // whole.
    const auto marker = std::ranges::find(args, "--");
    if (marker != args.end()) {
        const std::vector<std::string> head(args.begin(), marker);
        const std::vector<std::string> tail(marker + 1, args.end());
        if (const auto result = parse_and_run(head, tail, true, out, err, context)) {
            return *result;
        }
    }
    return *parse_and_run(args, {}, false, out, err, context);
}

}  // namespace bucu::cli
