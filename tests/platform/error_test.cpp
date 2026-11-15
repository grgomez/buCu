#include "bucu/platform/error.hpp"

#include <cstdint>
#include <type_traits>

#include "bucu/platform/paths.hpp"
#include "bucu/platform/terminal.hpp"
#include "catch_amalgamated.hpp"

// The enums of the public contract are one byte each (module-006).
static_assert(std::is_same_v<std::underlying_type_t<bucu::platform::ErrorCode>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<bucu::platform::Os>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<bucu::platform::ColorMode>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<bucu::platform::TerminalStatus>, std::uint8_t>);

TEST_CASE("a platform error holds its code and message", "[platform][error]") {
    const bucu::platform::Error error{bucu::platform::ErrorCode::kMissingVariable, "text"};
    CHECK(error.code == bucu::platform::ErrorCode::kMissingVariable);
    CHECK(error.message == "text");
}
