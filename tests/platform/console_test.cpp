#include "bucu/platform/console.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

#include "catch_amalgamated.hpp"
#include "platform/test_support.hpp"

namespace {

namespace test = bucu::platform::test;

constexpr unsigned int kUtf8CodePage = 65001;
constexpr unsigned int kVirtualTerminalProcessing = 0x0004;  // ENABLE_VIRTUAL_TERMINAL_PROCESSING

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

TEST_CASE("ConsoleSetup turns on escape sequence processing for a console and restores the mode",
          "[platform][console]") {
    const std::optional<unsigned int> before = test::console_output_mode();
    if (!before.has_value()) {
        SKIP("standard output is not a console");
    }
    {
        const bucu::platform::ConsoleSetup guard;
        CHECK((test::console_output_mode().value() & kVirtualTerminalProcessing) != 0);
    }
    CHECK(test::console_output_mode() == before);
}

// Hidden: run by hand (#8) to check that Ctrl+C restores the code page. It holds the
// guard and waits, so an automated run never selects it.
TEST_CASE("ConsoleSetup is held until the run is interrupted", "[.console-wait]") {
    const bucu::platform::ConsoleSetup guard;
    std::cout << "ConsoleSetup is active; press Ctrl+C, then run chcp.\n" << std::flush;
    std::this_thread::sleep_for(std::chrono::minutes(2));
}
