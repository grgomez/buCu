#include "bucu/platform/terminal.hpp"

#include <type_traits>

#include "catch_amalgamated.hpp"
#include "fake_environment.hpp"

namespace {

using bucu::platform::ColorMode;
using bucu::platform::is_interactive;
using bucu::platform::StandardStreams;
using bucu::platform::TerminalStatus;
using bucu::platform::use_color;
using bucu::platform::test::FakeEnvironment;

constexpr TerminalStatus kTerminal = TerminalStatus::kTerminal;
constexpr TerminalStatus kNotTerminal = TerminalStatus::kNotTerminal;

}  // namespace

// A raw boolean or integer never becomes a TerminalStatus by itself (US3-AC4).
static_assert(!std::is_convertible_v<bool, TerminalStatus>);
static_assert(!std::is_convertible_v<int, TerminalStatus>);
static_assert(!std::is_convertible_v<TerminalStatus, bool>);
static_assert(!std::is_convertible_v<bool, StandardStreams>);

TEST_CASE("auto colour needs a terminal and no NO_COLOR (product section 5)",
          "[platform][terminal]") {
    CHECK(use_color(ColorMode::kAuto, kTerminal, FakeEnvironment()));
    CHECK(use_color(ColorMode::kAuto, kTerminal, FakeEnvironment({{"NO_COLOR", ""}})));
    CHECK_FALSE(use_color(ColorMode::kAuto, kTerminal, FakeEnvironment({{"NO_COLOR", "1"}})));
    CHECK_FALSE(use_color(ColorMode::kAuto, kNotTerminal, FakeEnvironment()));
    CHECK_FALSE(use_color(ColorMode::kAuto, kNotTerminal, FakeEnvironment({{"NO_COLOR", "1"}})));
}

TEST_CASE("always colour ignores the terminal and NO_COLOR", "[platform][terminal]") {
    CHECK(use_color(ColorMode::kAlways, kNotTerminal, FakeEnvironment()));
    CHECK(use_color(ColorMode::kAlways, kTerminal, FakeEnvironment({{"NO_COLOR", "1"}})));
    CHECK(use_color(ColorMode::kAlways, kNotTerminal, FakeEnvironment({{"NO_COLOR", "1"}})));
}

TEST_CASE("never colour is off everywhere", "[platform][terminal]") {
    CHECK_FALSE(use_color(ColorMode::kNever, kTerminal, FakeEnvironment()));
    CHECK_FALSE(use_color(ColorMode::kNever, kNotTerminal, FakeEnvironment({{"NO_COLOR", ""}})));
}

TEST_CASE("a run is interactive only when input and output are both terminals",
          "[platform][terminal]") {
    CHECK(is_interactive({.input = kTerminal, .output = kTerminal}));
    CHECK_FALSE(is_interactive({.input = kTerminal, .output = kNotTerminal}));
    CHECK_FALSE(is_interactive({.input = kNotTerminal, .output = kTerminal}));
    CHECK_FALSE(is_interactive({.input = kNotTerminal, .output = kNotTerminal}));
}

TEST_CASE("the terminal wrappers can be called", "[platform][terminal]") {
    // Their value depends on how the tests run, so this asserts nothing about it.
    const TerminalStatus in = bucu::platform::stdin_is_terminal();
    const TerminalStatus out = bucu::platform::stdout_is_terminal();
    SUCCEED("stdin " << (in == kTerminal) << ", stdout " << (out == kTerminal));
}
