#include "cli/run.hpp"

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

}  // namespace

TEST_CASE("--version prints the version and exits 0 (product section 5)", "[cli]") {
    const Result result = invoke({"--version"});
    CHECK(result.out == "bucu 0.1.0\n");
    CHECK(result.err.empty());
    CHECK(result.code == 0);
}
