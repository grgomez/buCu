#include <cstdint>
#include <limits>

#include "bucu/core/bookmark.hpp"
#include "catch_amalgamated.hpp"
// rapidcheck/catch.h needs Catch2's header first.
#include <rapidcheck/catch.h>

namespace {

using bucu::core::clear_flag;
using bucu::core::Flag;
using bucu::core::FlagSet;
using bucu::core::set_flag;

// Any 64-bit pattern, with the highest bit and the all-ones pattern always in reach: Buku stores
// the integer bit for bit, and BuCu keeps the bits it does not know (product section 4 Flags).
rc::Gen<std::int64_t> bits() {
    return rc::gen::oneOf(rc::gen::arbitrary<std::int64_t>(),
                          rc::gen::just(std::numeric_limits<std::int64_t>::min()),
                          rc::gen::just(std::int64_t{-1}), rc::gen::just(std::int64_t{0}));
}

}  // namespace

// Product section 4 Flags: "Changing one flag MUST keep every other bit".
TEST_CASE("property: setting a flag sets its bits and keeps every other bit (product 4 Flags)",
          "[core][bookmark][property]") {
    rc::prop("set_flag gives bits | flag, and is idempotent", [] {
        const std::int64_t stored = *bits();
        const std::int64_t mask = *bits();
        const FlagSet set = set_flag(FlagSet(stored), Flag(mask));
        RC_ASSERT(set.bits() == (stored | mask));
        RC_ASSERT((set.bits() & ~mask) == (stored & ~mask));
        RC_ASSERT(set_flag(set, Flag(mask)) == set);
    });
}

TEST_CASE("property: clearing a flag clears its bits and keeps every other bit (product 4 Flags)",
          "[core][bookmark][property]") {
    rc::prop("clear_flag gives bits & ~flag, and is idempotent", [] {
        const std::int64_t stored = *bits();
        const std::int64_t mask = *bits();
        const FlagSet cleared = clear_flag(FlagSet(stored), Flag(mask));
        RC_ASSERT(cleared.bits() == (stored & ~mask));
        RC_ASSERT((cleared.bits() & ~mask) == (stored & ~mask));
        RC_ASSERT(clear_flag(cleared, Flag(mask)) == cleared);
    });
}
