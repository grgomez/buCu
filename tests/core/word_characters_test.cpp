#include "core/word_characters.hpp"

#include "catch_amalgamated.hpp"

namespace {

using bucu::core::is_word_character;

}  // namespace

TEST_CASE("Letters, digits, and underscore are word characters", "[core][word]") {
    CHECK(is_word_character(U'a'));
    CHECK(is_word_character(U'Z'));
    CHECK(is_word_character(U'0'));
    CHECK(is_word_character(U'_'));
    CHECK(is_word_character(U'é'));
    CHECK(is_word_character(U'Ж'));
}

TEST_CASE("Letters of other scripts are word characters", "[core][word]") {
    CHECK(is_word_character(U'中'));  // CJK, the end of a First/Last range
    CHECK(is_word_character(U'一'));  // first CJK ideograph
    CHECK(is_word_character(U'鿿'));  // inside the range
    CHECK(is_word_character(U'가'));  // Hangul syllable range start
    CHECK(is_word_character(U'힣'));  // Hangul syllable range end
    CHECK(is_word_character(U'あ'));
}

TEST_CASE("Numbers of every kind are word characters", "[core][word]") {
    CHECK(is_word_character(U'²'));  // No: superscript two
    CHECK(is_word_character(U'Ⅻ'));  // Nl: Roman numeral twelve
    CHECK(is_word_character(U'٣'));  // Nd: Arabic-Indic digit three
}

TEST_CASE("Marks, punctuation, symbols, and separators are not word characters", "[core][word]") {
    CHECK_FALSE(is_word_character(U'́'));  // Mn: combining acute accent
    CHECK_FALSE(is_word_character(U'-'));
    CHECK_FALSE(is_word_character(U' '));
    CHECK_FALSE(is_word_character(U'+'));
    CHECK_FALSE(is_word_character(U'.'));
    CHECK_FALSE(is_word_character(U'—'));  // Pd: em dash
    CHECK_FALSE(is_word_character(U' '));  // Zs: no-break space
    CHECK_FALSE(is_word_character(U'\n'));
    CHECK_FALSE(is_word_character(U'\U0001F600'));  // So: emoji
}

TEST_CASE("The code points around a range edge are decided separately", "[core][word]") {
    CHECK_FALSE(is_word_character(U'/'));    // right before 0
    CHECK_FALSE(is_word_character(U':'));    // right after 9
    CHECK_FALSE(is_word_character(U'`'));    // right after Z... before a
    CHECK_FALSE(is_word_character(U'{'));    // right after z
    CHECK_FALSE(is_word_character(U'힤'));  // right after the Hangul syllables
}
