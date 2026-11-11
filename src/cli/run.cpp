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

}  // namespace

int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
    if (args.empty()) {
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
    } catch (const CLI::ParseError& error) {
        return app.exit(error, out, err);
    }
    return 0;
}

}  // namespace bucu::cli
