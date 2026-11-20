#include "bucu/core/id_selection.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"

namespace {

using bucu::core::BookmarkId;
using bucu::core::covered_ranges;
using bucu::core::IdItem;
using bucu::core::IdItemKind;
using bucu::core::IdRange;
using bucu::core::IdSelection;
using bucu::core::named_id_count;
using bucu::core::parse_id_selection;

constexpr std::int64_t kMaxId = std::numeric_limits<std::int64_t>::max();

IdSelection parsed(const std::vector<std::string>& arguments) {
    const auto result = parse_id_selection(arguments);
    REQUIRE(result.has_value());
    return *result;
}

IdRange range(std::int64_t first, std::int64_t last) {
    return IdRange{.first = BookmarkId(first), .last = BookmarkId(last)};
}

IdItem single(std::int64_t id) {
    return IdItem{.kind = IdItemKind::kSingle, .range = range(id, id)};
}

IdItem span(std::int64_t first, std::int64_t last) {
    return IdItem{.kind = IdItemKind::kRange, .range = range(first, last)};
}

std::vector<IdRange> covered(const std::vector<std::string>& arguments) {
    return covered_ranges(parsed(arguments));
}

}  // namespace

TEST_CASE("no argument selects every bookmark (product section 5, IDs and ranges)", "[core][id]") {
    const IdSelection selection = parsed({});

    CHECK(selection.all);
    CHECK(selection.items.empty());
    CHECK(covered_ranges(selection) == std::vector<IdRange>{range(1, kMaxId)});
    CHECK(named_id_count(selection) == 0);
}

TEST_CASE("one ID is a single item", "[core][id]") {
    const IdSelection selection = parsed({"7"});

    CHECK_FALSE(selection.all);
    CHECK(selection.items == std::vector<IdItem>{single(7)});
    CHECK(named_id_count(selection) == 1);
}

TEST_CASE("an ID may have leading zeros and may be the largest 64-bit integer", "[core][id]") {
    CHECK(parsed({"007"}).items == std::vector<IdItem>{single(7)});
    CHECK(parsed({"9223372036854775807"}).items == std::vector<IdItem>{single(kMaxId)});
}

TEST_CASE("a comma list and several arguments give the same items", "[core][id]") {
    const std::vector<IdItem> expected = {single(3), single(5), single(9)};

    CHECK(parsed({"3,5,9"}).items == expected);
    CHECK(parsed({"3,5", "9"}).items == expected);
    CHECK(parsed({"9", "3", "5"}).items == expected);
}

TEST_CASE("a range is one item, and a reversed range is read as its swap", "[core][id]") {
    CHECK(parsed({"3-6"}).items == std::vector<IdItem>{span(3, 6)});
    CHECK(parsed({"11-8"}).items == std::vector<IdItem>{span(8, 11)});
    CHECK(parsed({"5-5"}).items == std::vector<IdItem>{span(5, 5)});
}

TEST_CASE("items are sorted by low end, a single before a range, and none repeats", "[core][id]") {
    const IdSelection selection = parsed({"9", "5-7", "5", "9", "5-7", "2"});

    CHECK(selection.items == std::vector<IdItem>{single(2), single(5), span(5, 7), single(9)});
}

TEST_CASE("a duplicate ID counts once", "[core][id]") {
    CHECK(named_id_count(parsed({"5,5"})) == 1);
    CHECK(named_id_count(parsed({"5", "5"})) == 1);
    CHECK(named_id_count(parsed({"5-5"})) == 1);
    CHECK(named_id_count(parsed({"5", "3-6"})) == 4);
    CHECK(named_id_count(parsed({"1-3", "2-5", "9"})) == 6);
}

TEST_CASE("the count covers a range that is far larger than memory", "[core][id]") {
    CHECK(named_id_count(parsed({"1-9223372036854775807"})) == static_cast<std::uint64_t>(kMaxId));
}

TEST_CASE("covered ranges are ascending and neither overlap nor touch", "[core][id]") {
    CHECK(covered({"3,5", "11-8"}) == std::vector<IdRange>{range(3, 3), range(5, 5), range(8, 11)});
    CHECK(covered({"1-4", "3-6", "9"}) == std::vector<IdRange>{range(1, 6), range(9, 9)});
    CHECK(covered({"4", "5", "6"}) == std::vector<IdRange>{range(4, 6)});
    CHECK(covered({"1-9999999999"}) == std::vector<IdRange>{range(1, 9999999999)});
    CHECK(covered({"5-9", "6"}) == std::vector<IdRange>{range(5, 9)});
}

TEST_CASE("covered ranges handle the largest ID without overflow", "[core][id]") {
    CHECK(covered({"9223372036854775806", "9223372036854775807"}) ==
          std::vector<IdRange>{range(kMaxId - 1, kMaxId)});
}

TEST_CASE("an argument that is not a positive whole number returns the whole argument",
          "[core][id]") {
    for (const char* argument : {"abc",
                                 "0",
                                 "-3",
                                 "1-x",
                                 "3,",
                                 ",3",
                                 "3,,5",
                                 "",
                                 " 3",
                                 "3 ",
                                 "+3",
                                 "3-",
                                 "-",
                                 "1-2-3",
                                 "0-5",
                                 "5-0",
                                 "99999999999999999999",
                                 "9223372036854775808",
                                 "1-9223372036854775808",
                                 "3,abc",
                                 "1e3",
                                 "\xc2\xb3",
                                 "3\n"}) {
        CAPTURE(argument);
        const auto result = parse_id_selection({"7", argument, "8"});

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().argument == argument);
    }
}

TEST_CASE("the first bad argument is the one returned", "[core][id]") {
    const auto result = parse_id_selection({"1", "x", "y"});

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().argument == "x");
}

TEST_CASE("parse_single_id reads one ID, with leading zeros", "[core][id]") {
    for (const char* text : {"7", "007"}) {
        CAPTURE(text);
        const auto result = bucu::core::parse_single_id(text);

        REQUIRE(result.has_value());
        CHECK(result->value() == 7);
    }
}

TEST_CASE("parse_single_id tells a list or a range from a bad item", "[core][id]") {
    using bucu::core::SingleIdError;
    for (const char* text : {"3,5", "3,3", "3-4", "3-3", "4-3"}) {
        CAPTURE(text);
        const auto result = bucu::core::parse_single_id(text);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == SingleIdError::kListOrRange);
    }
    for (const char* text : {"abc", "0", "-3", "", "99999999999999999999", "3,abc", " 3"}) {
        CAPTURE(text);
        const auto result = bucu::core::parse_single_id(text);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == SingleIdError::kNotAnIdItem);
    }
}
