#include "cli/run.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"

namespace {

struct Result {
    int code;
    std::string out;
    std::string err;
};

Result invoke(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = bucu::cli::run(args, out, err);
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
