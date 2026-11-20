#include "cli/record.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "bucu/core/bookmark.hpp"
#include "catch_amalgamated.hpp"

namespace {

std::string read_golden(const std::string& name) {
    std::ifstream file(std::string(BUCU_GOLDEN_DIR) + "/" + name, std::ios::binary);
    REQUIRE(file.is_open());
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bucu::core::Bookmark make(const char* title, const char* description, const char* tags) {
    auto bookmark =
        bucu::core::make_bookmark({.url = bucu::core::RequiredText{"https://example.org/"},
                                   .title = bucu::core::RequiredText{title},
                                   .description = bucu::core::RequiredText{description},
                                   .tags = tags});
    REQUIRE(bookmark.has_value());
    return std::move(*bookmark);
}

// The record without colour.
std::string plain(bucu::core::BookmarkId id, const bucu::core::Bookmark& bookmark) {
    return bucu::cli::format_record(id, bookmark, false);
}

constexpr const char* kEsc = "\x1b[";

}  // namespace

TEST_CASE(
    "a coloured record equals the colour golden file with Buku's colours and a reset before each "
    "LF",
    "[cli]") {
    auto bookmark = make("First\nsecond", "d1\nd2", "web, dev");
    bookmark.flags = bucu::core::kFlagImmutableTitle;

    CHECK(bucu::cli::format_record(bucu::core::BookmarkId(7), bookmark, true) ==
          read_golden("record_colour.txt"));
}

TEST_CASE("a coloured record with no title colours the ID alone, with the reset before the LF",
          "[cli]") {
    CHECK(bucu::cli::format_record(bucu::core::BookmarkId(7), make("", "", ""), true) ==
          std::string(kEsc) + "96m7" + kEsc + "0m\n" + kEsc + "91m   url:   " + kEsc + "0m" + kEsc +
              "93mhttps://example.org/" + kEsc + "0m\n");
}

TEST_CASE("a coloured record escapes controls and holds no raw escape byte but its own codes",
          "[cli]") {
    const std::string record =
        bucu::cli::format_record(bucu::core::BookmarkId(7), make("a\x1b[31mb", "", ""), true);

    CHECK(record.find("a\\x1b[31mb") != std::string::npos);
    CHECK(record.find("a\x1b[31mb") == std::string::npos);
}

TEST_CASE("a plain record holds no escape sequence", "[cli]") {
    auto bookmark = make("First\nsecond", "d1\nd2", "web, dev");
    bookmark.flags = bucu::core::kFlagImmutableTitle;

    CHECK(plain(bucu::core::BookmarkId(7), bookmark).find('\x1b') == std::string::npos);
}

TEST_CASE("a record shows every field in a labelled block (product section 5)", "[cli]") {
    auto bookmark = make("Example", "A page", "web dev, docs, tools");
    bookmark.flags = bucu::core::kFlagImmutableTitle;

    CHECK(plain(bucu::core::BookmarkId(7), bookmark) == read_golden("record_full.txt"));
}

TEST_CASE("a record with no title, description, tags, or flag has only the ID and the URL",
          "[cli]") {
    CHECK(plain(bucu::core::BookmarkId(7), make("", "", "")) == read_golden("record_id_only.txt"));
}

TEST_CASE("a line break in the title or description continues at the value column", "[cli]") {
    CHECK(plain(bucu::core::BookmarkId(7), make("First line\nsecond line", "one\ntwo\nthree",
                                                "")) == read_golden("record_multiline.txt"));
}

TEST_CASE("the flag line names the immutable title and nothing else", "[cli]") {
    auto bookmark = make("", "", "");
    bookmark.flags = bucu::core::kFlagImmutableTitle;

    CHECK(plain(bucu::core::BookmarkId(7), bookmark) == read_golden("record_flag.txt"));
}

TEST_CASE("control characters in the title and description are written as escapes, LF excepted",
          "[cli]") {
    const auto bookmark = make("a\x1b[31mb\tc", "one\rtwo\nthree\x7f\xc2\x85", "");

    CHECK(plain(bucu::core::BookmarkId(7), bookmark) ==
          "7  a\\x1b[31mb\\tc\n"
          "   url:   https://example.org/\n"
          "   desc:  one\\rtwo\n"
          "          three\\x7f\\u0085\n");
}

TEST_CASE("a line break in a title keeps the continuation-line rule while a control is escaped",
          "[cli]") {
    CHECK(plain(bucu::core::BookmarkId(7), make("A\x1b\nB", "", "")) ==
          "7  A\\x1b\n"
          "          B\n"
          "   url:   https://example.org/\n");
}

TEST_CASE("every line of a record ends in LF and never CR LF", "[cli]") {
    const std::string record = plain(bucu::core::BookmarkId(7), make("T", "d\ne", "a"));

    CHECK(record.ends_with('\n'));
    CHECK(record.find("\r\n") == std::string::npos);
}
