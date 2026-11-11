#include "cli/run.hpp"

#include <ostream>
#include <string>
#include <vector>

#include "CLI11.hpp"

namespace bucu::cli {

int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
    // An empty name keeps CLI11 from prefixing its messages with the program name.
    CLI::App app{"", ""};
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
