#include "bucu/platform/console.hpp"

#include "catch_amalgamated.hpp"
#include "platform/test_support.hpp"

namespace {

namespace test = bucu::platform::test;

constexpr unsigned int kUtf8CodePage = 65001;

}  // namespace

TEST_CASE("ConsoleSetup is harmless to create and destroy", "[platform][console]") {
    { const bucu::platform::ConsoleSetup guard; }
    SUCCEED();
}

TEST_CASE("ConsoleSetup selects the UTF-8 code page and restores the previous one",
          "[platform][console]") {
    if (!test::console_attached()) {
        SKIP("no console is attached");
    }
    const unsigned int before = test::console_output_code_page();
    {
        const bucu::platform::ConsoleSetup guard;
        CHECK(test::console_output_code_page() == kUtf8CodePage);
    }
    CHECK(test::console_output_code_page() == before);
}
