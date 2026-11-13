#include "bucu/core/url.hpp"

#include <string>
#include <string_view>

#include "bucu/core/error.hpp"
#include "catch_amalgamated.hpp"
#include "core/utf8.hpp"

namespace {

using bucu::core::Field;
using bucu::core::normalize_url;
using bucu::core::Reason;

std::string utf8(std::u32string_view code_points) {
    std::string out;
    for (const char32_t cp : code_points) {
        bucu::core::append_utf8(out, cp);
    }
    return out;
}

void check_accepted(std::string_view url) {
    INFO("url: " << url);
    const auto result = normalize_url(url);
    REQUIRE(result.has_value());
    CHECK(*result == url);
}

void check_refused(std::string_view url, Reason reason) {
    INFO("url: " << url);
    const auto result = normalize_url(url);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().field == Field::kUrl);
    CHECK(result.error().reason == reason);
    CHECK(result.error().value == url);
}

}  // namespace

TEST_CASE("URL validation accepts schemes browsers store", "[core][url]") {
    check_accepted("about:blank");
    check_accepted("place:sort=8");
    check_accepted("mailto:a@example.org");
    check_accepted("file:///home/a.html");
    check_accepted("javascript:void(0)");
    check_accepted("moz-extension://abc/page.html");
    check_accepted("https://example.org/a?b=c#d");
}

TEST_CASE("URL validation trims leading and trailing whitespace", "[core][url]") {
    CHECK(normalize_url("  about:blank\t\n").value() == "about:blank");
    CHECK(normalize_url(utf8(U"\u3000about:blank\u00A0")).value() == "about:blank");
}

TEST_CASE("URL validation refuses an empty URL", "[core][url]") {
    check_refused("", Reason::kEmpty);
    check_refused(" \t ", Reason::kEmpty);
}

TEST_CASE("URL validation refuses a URL with no scheme", "[core][url]") {
    check_refused("example.org", Reason::kNoScheme);
    check_refused("localhost:8080", Reason::kNoScheme);
    check_refused("example.org:8080/x", Reason::kNoScheme);
    check_refused("C:\\Users\\a.html", Reason::kNoScheme);
    check_refused("c:/a.html", Reason::kNoScheme);
    check_refused("/a/b", Reason::kNoScheme);
    check_refused("1http://a.org", Reason::kNoScheme);
    check_refused("ht tp://a.org", Reason::kWhitespace);
    check_refused("http:", Reason::kNoScheme);
    check_refused(":a", Reason::kNoScheme);
}

TEST_CASE("URL validation treats a port-like rest as no scheme only when it ends or has a slash",
          "[core][url]") {
    check_accepted("a1:80x");  // digits then a letter: a scheme
    check_refused("host:80", Reason::kNoScheme);
    check_refused("host:80/", Reason::kNoScheme);
}

TEST_CASE("URL validation refuses an empty host", "[core][url]") {
    check_refused("https://", Reason::kEmptyHost);
    check_refused("https:///path", Reason::kEmptyHost);
    check_refused("https://:8080/x", Reason::kEmptyHost);
    check_refused("http://user@/x", Reason::kEmptyHost);
    check_refused("https://?q=1", Reason::kEmptyHost);
    check_accepted("file://");
    check_accepted("FILE:///x");
}

TEST_CASE("URL validation refuses whitespace and control characters inside", "[core][url]") {
    check_refused("https://a.org/a b", Reason::kWhitespace);
    check_refused("https://a.org/a\tb", Reason::kWhitespace);
    check_refused(utf8(U"https://a.org/a\u00A0b"), Reason::kWhitespace);
    check_refused(utf8(U"https://a.org/a\u3000b"), Reason::kWhitespace);
    check_refused(
        "https://a.org/a\x01"
        "b",
        Reason::kControlCharacter);
    check_refused(
        "https://a.org/a\x7F"
        "b",
        Reason::kControlCharacter);
    check_refused(std::string("https://a.org/a\0b", 17), Reason::kControlCharacter);
    check_refused(utf8(U"https://a.org/a\u0085b"), Reason::kWhitespace);
    check_refused(utf8(U"https://a.org/a\u0090b"), Reason::kControlCharacter);
}

TEST_CASE("URL validation refuses invalid UTF-8", "[core][url]") {
    check_refused("https://a.org/\xC3", Reason::kInvalidUtf8);
    check_refused("https://a.org/\xFF\xFE", Reason::kInvalidUtf8);
    check_refused("\xC0\xAF", Reason::kInvalidUtf8);
}

TEST_CASE("URL validation accepts non-ASCII text", "[core][url]") {
    check_accepted(utf8(U"https://a.org/\u00F1and\u00FA"));
}
