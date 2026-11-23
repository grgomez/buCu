#include "bucu/core/search.hpp"

#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "core/utf8.hpp"

namespace {

using bucu::core::Field;
using bucu::core::parse_search_query;
using bucu::core::Reason;

using Words = std::vector<std::string>;

std::string utf8(std::u32string_view code_points) {
    std::string out;
    for (const char32_t cp : code_points) {
        bucu::core::append_utf8(out, cp);
    }
    return out;
}

Words keywords(const Words& arguments) {
    const auto query = parse_search_query(arguments, false, false);
    REQUIRE(query.has_value());
    return query->keywords;
}

}  // namespace

TEST_CASE("Search parsing trims, collapses, lowercases, and keeps the order", "[core][search]") {
    CHECK(keywords({"  Web  "}) == Words{"web"});
    CHECK(keywords({"web  \t dev"}) == Words{"web dev"});
    CHECK(keywords({"Docs", "web"}) == Words{"docs", "web"});
    CHECK(keywords({" Été "}) == Words{utf8(U"été")});
}

TEST_CASE("Search parsing drops empty keywords and counts repeats once", "[core][search]") {
    CHECK(keywords({"", " ", "web", "\t"}) == Words{"web"});
    CHECK(keywords({"Web", "web", " WEB "}) == Words{"web"});
    CHECK(keywords({"web dev", "web   dev"}) == Words{"web dev"});
}

TEST_CASE("Search parsing keeps control characters and the switches", "[core][search]") {
    CHECK(keywords({"a\x01"
                    "b"}) == Words{"a\x01"
                                   "b"});
    const auto query = parse_search_query({"web"}, true, true);
    REQUIRE(query.has_value());
    CHECK(query->all);
    CHECK(query->deep);
    const auto plain = parse_search_query({"web"}, false, false);
    REQUIRE(plain.has_value());
    CHECK_FALSE(plain->all);
    CHECK_FALSE(plain->deep);
}

TEST_CASE("Search parsing refuses an empty keyword list", "[core][search]") {
    for (const Words& arguments : {Words{}, Words{""}, Words{"", " ", "\t\n"}}) {
        const auto query = parse_search_query(arguments, false, false);
        REQUIRE_FALSE(query.has_value());
        CHECK(query.error().field == Field::kKeyword);
        CHECK(query.error().reason == Reason::kEmpty);
    }
}

TEST_CASE("Search parsing refuses a keyword that is not valid UTF-8", "[core][search]") {
    const auto query = parse_search_query({"web", "\xFF"}, false, false);
    REQUIRE_FALSE(query.has_value());
    CHECK(query.error().field == Field::kKeyword);
    CHECK(query.error().reason == Reason::kInvalidUtf8);
    const auto lone = parse_search_query({"", "\xC3"}, false, false);
    REQUIRE_FALSE(lone.has_value());
    CHECK(lone.error().reason == Reason::kInvalidUtf8);
}
