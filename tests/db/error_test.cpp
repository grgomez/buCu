#include "bucu/db/error.hpp"

#include <cstdint>
#include <type_traits>

#include "catch_amalgamated.hpp"

// The enums of the public contract are one byte each (module-006).
static_assert(std::is_same_v<std::underlying_type_t<bucu::db::ErrorCode>, std::uint8_t>);

TEST_CASE("a schema version compares by its number", "[db][error]") {
    CHECK(bucu::db::SchemaVersion(1) == bucu::db::kSchemaVersion);
    CHECK(bucu::db::SchemaVersion(0) < bucu::db::kSchemaVersion);
    CHECK(bucu::db::SchemaVersion(7).value() == 7);
}
