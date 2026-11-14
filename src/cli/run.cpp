#include "cli/run.hpp"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "CLI11.hpp"

namespace bucu::cli {

namespace {

constexpr const char* kHelpText = R"(bucu - a command-line bookmark manager

Usage:
  bucu [options]

Options:
  -h, --help     Print this help text and exit.
      --version  Print the version and exit.

Exit codes:
  0  success
  1  a runtime failure
  2  a usage error
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
        return 1;
    }
    return code;
}

}  // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err,
        const Context& /*context*/) {
    // A lone "--" has nothing after it to run, so it counts as a bare call.
    if (args.empty() || (args.size() == 1 && args[0] == "--")) {
        err << kHelpText;
        return 2;
    }

    // An empty name keeps CLI11 from prefixing its messages with the program name.
    CLI::App app{"", ""};
    app.formatter(std::make_shared<HelpFormatter>());
    app.set_version_flag("--version", "bucu " BUCU_VERSION);

    // CLI11 reads the arguments from the back of the vector.
    std::vector<std::string> reversed(args.rbegin(), args.rend());
    try {
        app.parse(reversed);
    } catch (const CLI::CallForHelp& help) {
        return finish(app.exit(help, out, err), out, err);
    } catch (const CLI::CallForVersion& version) {
        return finish(app.exit(version, out, err), out, err);
    } catch (const CLI::ParseError& error) {
        std::string what = error.what();
        if (what.ends_with('.')) {
            what.pop_back();
        }
        err << "bucu: " << what << ". Run 'bucu --help' for usage.\n";
        return 2;
    }
    return finish(0, out, err);
}

}  // namespace bucu::cli
