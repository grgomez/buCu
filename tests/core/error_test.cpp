#include "bucu/core/error.hpp"

#include <cstdint>
#include <type_traits>

#include "catch_amalgamated.hpp"

// The enums of the public contract are one byte each (module-006).
static_assert(std::is_same_v<std::underlying_type_t<bucu::core::Field>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<bucu::core::Reason>, std::uint8_t>);

TEST_CASE("a validation error holds the field, the reason, and the value", "[core][error]") {
    const bucu::core::ValidationError error{
        .field = bucu::core::Field::kUrl, .reason = bucu::core::Reason::kEmpty, .value = "x"};
    CHECK(error.field == bucu::core::Field::kUrl);
    CHECK(error.reason == bucu::core::Reason::kEmpty);
    CHECK(error.value == "x");
}
