#include <string>
#include <string_view>
#include <vector>

#include "bucu/core/search.hpp"
#include "catch_amalgamated.hpp"
// rapidcheck/catch.h needs Catch2's header first.
#include <rapidcheck/catch.h>

#include "property_support.hpp"

namespace {

using bucu::core::count_matching_keywords;
using bucu::core::parse_search_query;
using bucu::core::test::encode;

// How large the generated keyword and field texts get (the generator size, not a rule).
constexpr int kTextSize = 10;

// Text over a small alphabet, so keywords often occur in fields: letters, digits, '_', a space,
// punctuation, an accented letter, and a combining mark.
rc::Gen<std::string> small_text() {
    const std::vector<char32_t> alphabet = {U'a', U'b', U'1', U'_', U' ', U'-', U'.', U'é', U'́'};
    return rc::gen::map(rc::gen::container<std::vector<char32_t>>(rc::gen::elementOf(alphabet)),
                        [](const std::vector<char32_t>& cps) { return encode(cps); });
}

}  // namespace

// Product section 5 search: a whole-word match is a match of the deep rule with a condition on
// its neighbours, so the whole-word count never exceeds the deep count.
TEST_CASE("Search: the whole-word count never exceeds the deep count", "[core][search][property]") {
    rc::prop("whole-word count <= deep count", [] {
        const auto keywords =
            *rc::gen::resize(kTextSize, rc::gen::container<std::vector<std::string>>(small_text()));
        const auto fields =
            *rc::gen::resize(kTextSize, rc::gen::container<std::vector<std::string>>(small_text()));
        const std::vector<std::string_view> views(fields.begin(), fields.end());
        const auto whole = parse_search_query(keywords, false, false);
        const auto deep = parse_search_query(keywords, false, true);
        RC_ASSERT(whole.has_value() == deep.has_value());
        if (whole) {
            RC_ASSERT(count_matching_keywords(*whole, views) <=
                      count_matching_keywords(*deep, views));
        }
    });
}

// A keyword that is a whole field of its own is found as a whole word.
TEST_CASE("Search: a keyword equal to a field text matches it", "[core][search][property]") {
    rc::prop("keyword == field", [] {
        const auto text = *rc::gen::resize(kTextSize, small_text());
        const auto query = parse_search_query({text}, false, false);
        if (!query) {
            return;
        }
        const std::vector<std::string_view> views{query->keywords.front()};
        RC_ASSERT(count_matching_keywords(*query, views) == 1);
    });
}
