#include "cli/run.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "catch_amalgamated.hpp"
#include "platform/fake_environment.hpp"

namespace {

struct Result {
    int code;
    std::string out;
    std::string err;
};

// A context with no variables and a fixed clock, so no test depends on the machine.
const bucu::platform::test::FakeEnvironment kEnvironment;
const bucu::cli::Context kContext{bucu::platform::Os::kLinuxOrBsd, kEnvironment, 1700000000};

Result invoke(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = bucu::cli::run(args, out, err, kContext);
    return {code, out.str(), err.str()};
}

std::string read_golden(const std::string& name) {
    std::ifstream file(std::string(BUCU_GOLDEN_DIR) + "/" + name, std::ios::binary);
    REQUIRE(file.is_open());
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

}  // namespace

TEST_CASE("--version prints the version and exits 0 (product section 5)", "[cli]") {
    const Result result = invoke({"--version"});
    CHECK(result.out == "bucu 0.1.0\n");
    CHECK(result.err.empty());
    CHECK(result.code == 0);
}

TEST_CASE("--help and -h print the help text to standard output (product section 5)", "[cli]") {
    const std::string help = read_golden("help.txt");
    for (const char* flag : {"--help", "-h"}) {
        CAPTURE(flag);
        const Result result = invoke({flag});
        CHECK(result.out == help);
        CHECK(result.err.empty());
        CHECK(result.code == 0);
    }
}

TEST_CASE("help wins when other arguments are given (product section 5)", "[cli]") {
    const std::string help = read_golden("help.txt");
    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"--help", "--bogus"}, {"--bogus", "-h"}, {"extra", "--help"}}) {
        const Result result = invoke(args);
        CHECK(result.out == help);
        CHECK(result.err.empty());
        CHECK(result.code == 0);
    }
}

TEST_CASE("a bare call prints the help text to standard error and exits 2 (product section 6)",
          "[cli]") {
    const Result result = invoke({});
    CHECK(result.out.empty());
    CHECK(result.err == read_golden("help.txt"));
    CHECK(result.code == 2);
}

TEST_CASE("a usage error prints one line and exits 2 (product section 6)", "[cli]") {
    for (const auto& [argument, named] : {std::pair<const char*, const char*>{"--bogus", "--bogus"},
                                          {"extra", "extra"},
                                          {"--version=x", "--version"}}) {
        CAPTURE(argument);
        const Result result = invoke({argument});
        CHECK(result.out.empty());
        CHECK(result.code == 2);
        CHECK(result.err.starts_with("bucu: "));
        CHECK(result.err.find(named) != std::string::npos);
        CHECK(result.err.ends_with(". Run 'bucu --help' for usage.\n"));
        CHECK(std::count(result.err.begin(), result.err.end(), '\n') == 1);
    }
}

TEST_CASE("a failed write to standard output reports it and exits 1 (product section 6)", "[cli]") {
    for (const char* flag : {"--version", "--help"}) {
        CAPTURE(flag);
        std::ostringstream out;
        out.setstate(std::ios::badbit);
        std::ostringstream err;
        const int code = bucu::cli::run({flag}, out, err, kContext);
        CHECK(err.str() == "bucu: cannot write to standard output\n");
        CHECK(code == 1);
    }
}

TEST_CASE("--version wins over other arguments (product section 6)", "[cli]") {
    for (const std::vector<std::string>& args : {std::vector<std::string>{"--version", "extra"},
                                                 {"extra", "--version"},
                                                 {"--bogus", "--version"}}) {
        const Result result = invoke(args);
        CHECK(result.out == "bucu 0.1.0\n");
        CHECK(result.err.empty());
        CHECK(result.code == 0);
    }
}

TEST_CASE("a bare -- is a bare call (product section 6)", "[cli]") {
    const Result result = invoke({"--"});
    CHECK(result.out.empty());
    CHECK(result.err == read_golden("help.txt"));
    CHECK(result.code == 2);
}

TEST_CASE("help wins over --version in either order (product section 6)", "[cli]") {
    const std::string help = read_golden("help.txt");
    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"--version", "--help"}, {"--help", "--version"}}) {
        const Result result = invoke(args);
        CHECK(result.out == help);
        CHECK(result.err.empty());
        CHECK(result.code == 0);
    }
}
