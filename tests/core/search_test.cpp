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

namespace {

using bucu::core::count_matching_keywords;
using Fields = std::vector<std::string_view>;

std::size_t count(const Words& arguments, const Fields& fields, bool deep = false) {
    const auto query = parse_search_query(arguments, false, deep);
    REQUIRE(query.has_value());
    return count_matching_keywords(*query, fields);
}

}  // namespace

TEST_CASE("A keyword matches a whole word in any one field", "[core][search]") {
    CHECK(count({"pen"}, {"https://x.example/", "Pen review", "", ""}) == 1);
    CHECK(count({"rust"}, {"https://rust.example/", "", "", ""}) == 1);
    CHECK(count({"rust"}, {"https://x.example/", "", "", "rust"}) == 1);
    CHECK(count({"rust"}, {"https://x.example/", "", "about rust", ""}) == 1);
    CHECK(count({"rust"}, {"https://x.example/", "no match", "", ""}) == 0);
}

TEST_CASE("A whole-word match needs non-word characters or an edge on both sides",
          "[core][search]") {
    CHECK(count({"pen"}, {"Opens soon"}) == 0);
    CHECK(count({"pen"}, {"pens"}) == 0);
    CHECK(count({"pen"}, {"open pen"}) == 1);
    CHECK(count({"pen"}, {"pen_name"}) == 0);  // '_' is a word character
    CHECK(count({"pen"}, {"pen2"}) == 0);
    CHECK(count({"pen"}, {"a pen, b"}) == 1);
}

TEST_CASE("Every occurrence is tried, not only the first", "[core][search]") {
    CHECK(count({"pen"}, {"Opens pen"}) == 1);
    CHECK(count({"pen"}, {"pens pens pen pens"}) == 1);
    CHECK(count({"pen"}, {"pens pens"}) == 0);
}

TEST_CASE("Accented letters are word characters and a hyphen is not", "[core][search]") {
    CHECK(count({utf8(U"café")}, {utf8(U"café-bar")}) == 1);
    CHECK(count({utf8(U"café")}, {utf8(U"cafébar")}) == 0);
    CHECK(count({"caf"}, {utf8(U"café")}) == 0);
    CHECK(count({"bar"}, {utf8(U"ébar")}) == 0);
    CHECK(count({"bar"}, {utf8(U"́bar")}) == 1);  // a combining mark is not a word character
}

TEST_CASE("Matching is case-insensitive by the Unicode lowercase mapping", "[core][search]") {
    CHECK(count({utf8(U"ÉTÉ")}, {utf8(U"Été à Paris")}) == 1);
    CHECK(count({"paris"}, {utf8(U"Été à Paris")}) == 1);
    CHECK(count({"PARIS"}, {"paris"}) == 1);
    CHECK(count({utf8(U"ΣΑΣ")}, {utf8(U"σας")}) == 1);  // final sigma
    CHECK(count({"e"}, {utf8(U"É")}) == 0);             // no accent folding
}

TEST_CASE("A keyword phrase matches the field text as it is", "[core][search]") {
    CHECK(count({"web dev"}, {"web dev"}) == 1);
    CHECK(count({"web dev"}, {"web  dev"}) == 0);
    CHECK(count({"web dev"}, {"web-dev"}) == 0);
    CHECK(count({"web dev"}, {"the web dev guide"}) == 1);
    CHECK(count({"web dev"}, {"web devs"}) == 0);
    CHECK(count({"web dev"}, {"web", "dev"}) == 0);  // never across two fields
}

TEST_CASE("A keyword of non-word characters checks the characters around it", "[core][search]") {
    CHECK(count({"c++"}, {"C++ notes"}) == 1);
    CHECK(count({"c++"}, {"C+++"}) == 1);  // the character after is '+', not a word character
    CHECK(count({"."}, {"a.b"}) == 0);
    CHECK(count({"."}, {"a . b"}) == 1);
    CHECK(count({"."}, {"a.b"}, true) == 1);
}

TEST_CASE("Deep matching finds a keyword inside words", "[core][search]") {
    CHECK(count({"pen"}, {"Opens soon"}, true) == 1);
    CHECK(count({"pen"}, {"pen"}, true) == 1);
    CHECK(count({"caf"}, {utf8(U"café")}, true) == 1);
    CHECK(count({"xyz"}, {"abc"}, true) == 0);
}

TEST_CASE("The score counts distinct keywords once, however many fields match", "[core][search]") {
    CHECK(count({"web", "docs"}, {"web docs", "web", "docs", "web"}) == 2);
    CHECK(count({"web", "docs", "none"}, {"web", "docs"}) == 2);
    CHECK(count({"web"}, {"web", "web", "web"}) == 1);
    CHECK(count({"web"}, {}) == 0);
}

TEST_CASE("A field that is not valid UTF-8 matches nothing", "[core][search]") {
    CHECK(count({"a"}, {"a\xFF"}) == 0);
    CHECK(count({"a"}, {"a\xFF", "a"}) == 1);
}
