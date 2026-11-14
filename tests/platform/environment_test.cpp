#include "bucu/platform/environment.hpp"

#include <string>

#include "catch_amalgamated.hpp"
#include "platform/test_support.hpp"

namespace {

using bucu::platform::SystemEnvironment;
namespace test = bucu::platform::test;

// Names unique to BuCu's tests, so no real variable is touched.
constexpr const char* kName = "BUCU_TEST_ENVIRONMENT_VALUE";

}  // namespace

TEST_CASE("SystemEnvironment reads a set variable, including non-ASCII text", "[platform][env]") {
    const std::string value = "C:\\Users\\Jos\xC3\xA9\\\xE6\x97\xA5\xE6\x9C\xAC";  // José, 日本
    test::set_environment_variable(kName, value);

    const SystemEnvironment environment;
    const auto read = environment.get(kName);
    test::unset_environment_variable(kName);

    REQUIRE(read.has_value());
    CHECK(*read == value);
}

TEST_CASE("SystemEnvironment reports an unset variable as no value", "[platform][env]") {
    test::unset_environment_variable(kName);
    const SystemEnvironment environment;
    CHECK_FALSE(environment.get(kName).has_value());
}
