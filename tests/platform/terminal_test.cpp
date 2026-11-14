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
