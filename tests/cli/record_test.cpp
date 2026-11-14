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
    auto bookmark = bucu::core::make_bookmark("https://example.org/", title, description, tags);
    REQUIRE(bookmark.has_value());
    return std::move(*bookmark);
}

}  // namespace

TEST_CASE("a record shows every field in a labelled block (product section 5)", "[cli]") {
    auto bookmark = make("Example", "A page", "web dev, docs, tools");
    bookmark.flags = bucu::core::kFlagImmutableTitle;

    CHECK(bucu::cli::format_record(7, bookmark) == read_golden("record_full.txt"));
}

TEST_CASE("a record with no title, description, tags, or flag has only the ID and the URL",
          "[cli]") {
    CHECK(bucu::cli::format_record(7, make("", "", "")) == read_golden("record_id_only.txt"));
}

TEST_CASE("a line break in the title or description continues at the value column", "[cli]") {
    CHECK(bucu::cli::format_record(7, make("First line\nsecond line", "one\ntwo\nthree", "")) ==
          read_golden("record_multiline.txt"));
}

TEST_CASE("the flag line names the immutable title and nothing else", "[cli]") {
    auto bookmark = make("", "", "");
    bookmark.flags = bucu::core::kFlagImmutableTitle;

    CHECK(bucu::cli::format_record(7, bookmark) == read_golden("record_flag.txt"));
}

TEST_CASE("every line of a record ends in LF and never CR LF", "[cli]") {
    const std::string record = bucu::cli::format_record(7, make("T", "d\ne", "a"));

    CHECK(record.ends_with('\n'));
    CHECK(record.find("\r\n") == std::string::npos);
}
