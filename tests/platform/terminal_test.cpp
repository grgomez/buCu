#include "bucu/platform/terminal.hpp"

#include "catch_amalgamated.hpp"
#include "fake_environment.hpp"

namespace {

using bucu::platform::ColorMode;
using bucu::platform::use_color;
using bucu::platform::test::FakeEnvironment;

}  // namespace

TEST_CASE("auto colour needs a terminal and no NO_COLOR (product section 5)",
          "[platform][terminal]") {
    CHECK(use_color(ColorMode::kAuto, true, FakeEnvironment()));
    CHECK(use_color(ColorMode::kAuto, true, FakeEnvironment({{"NO_COLOR", ""}})));
    CHECK_FALSE(use_color(ColorMode::kAuto, true, FakeEnvironment({{"NO_COLOR", "1"}})));
    CHECK_FALSE(use_color(ColorMode::kAuto, false, FakeEnvironment()));
    CHECK_FALSE(use_color(ColorMode::kAuto, false, FakeEnvironment({{"NO_COLOR", "1"}})));
}

TEST_CASE("always colour ignores the terminal and NO_COLOR", "[platform][terminal]") {
    CHECK(use_color(ColorMode::kAlways, false, FakeEnvironment()));
    CHECK(use_color(ColorMode::kAlways, true, FakeEnvironment({{"NO_COLOR", "1"}})));
    CHECK(use_color(ColorMode::kAlways, false, FakeEnvironment({{"NO_COLOR", "1"}})));
}

TEST_CASE("never colour is off everywhere", "[platform][terminal]") {
    CHECK_FALSE(use_color(ColorMode::kNever, true, FakeEnvironment()));
    CHECK_FALSE(use_color(ColorMode::kNever, false, FakeEnvironment({{"NO_COLOR", ""}})));
}

TEST_CASE("a run is interactive only when input and output are both terminals",
          "[platform][terminal]") {
    CHECK(bucu::platform::is_interactive(true, true));
    CHECK_FALSE(bucu::platform::is_interactive(true, false));
    CHECK_FALSE(bucu::platform::is_interactive(false, true));
    CHECK_FALSE(bucu::platform::is_interactive(false, false));
}

TEST_CASE("the terminal wrappers can be called", "[platform][terminal]") {
    // Their value depends on how the tests run, so this asserts nothing about it.
    const bool in = bucu::platform::stdin_is_terminal();
    const bool out = bucu::platform::stdout_is_terminal();
    SUCCEED("stdin " << in << ", stdout " << out);
}
