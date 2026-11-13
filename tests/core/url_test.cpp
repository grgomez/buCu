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
    CHECK(normalize_url("file://").has_value());
    CHECK(normalize_url("FILE:///x").has_value());
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

namespace {

std::string normalized(std::string_view url) {
    const auto result = normalize_url(url);
    REQUIRE(result.has_value());
    return *result;
}

}  // namespace

TEST_CASE("URL normalization gives one form for the same address", "[core][url]") {
    CHECK(normalized("HTTPS://Example.ORG") == "https://example.org/");
    CHECK(normalized("https://example.org:443") == "https://example.org/");
    CHECK(normalized("https://example.org/") == "https://example.org/");
    CHECK(normalized(" https://EXAMPLE.org:443/ ") == "https://example.org/");
}

TEST_CASE("URL normalization keeps a trailing slash and the case of path, query, and fragment",
          "[core][url]") {
    CHECK(normalized("https://example.org/docs") == "https://example.org/docs");
    CHECK(normalized("https://example.org/docs/") == "https://example.org/docs/");
    CHECK(normalized("https://Example.org/Docs/A?Q=B#Frag") ==
          "https://example.org/Docs/A?Q=B#Frag");
    CHECK(normalized("https://example.org/a//b/../c/./d") == "https://example.org/a//b/../c/./d");
    CHECK(normalized("https://example.org/%7euser/%7Euser") ==
          "https://example.org/%7euser/%7Euser");
}

TEST_CASE("URL normalization drops only the default port of the scheme", "[core][url]") {
    CHECK(normalized("http://a.org:80/x") == "http://a.org/x");
    CHECK(normalized("ftp://a.org:21/x") == "ftp://a.org/x");
    CHECK(normalized("https://a.org:8443") == "https://a.org:8443/");
    CHECK(normalized("http://a.org:443/") == "http://a.org:443/");
    CHECK(normalized("https://a.org:80/") == "https://a.org:80/");
    CHECK(normalized("ssh://a.org:80/") == "ssh://a.org:80/");
    CHECK(normalized("http://a.org:080/") == "http://a.org:080/");
    CHECK(normalized("http://a.org:abc/") == "http://a.org:abc/");
    CHECK(normalized("http://a.org:/") == "http://a.org:/");
}

TEST_CASE("URL normalization puts the empty-path slash before a query or fragment", "[core][url]") {
    CHECK(normalized("https://example.org?q=1") == "https://example.org/?q=1");
    CHECK(normalized("https://example.org#top") == "https://example.org/#top");
    CHECK(normalized("https://example.org:443?q=1#top") == "https://example.org/?q=1#top");
    CHECK(normalized("https://example.org/?q=1") == "https://example.org/?q=1");
}

TEST_CASE("URL normalization lowercases a non-ASCII host with the Unicode mapping", "[core][url]") {
    CHECK(normalized(utf8(U"https://ÉXAMPLE.org")) == utf8(U"https://éxample.org/"));
    CHECK(normalized(utf8(U"https://example.org/É")) == utf8(U"https://example.org/É"));
}

TEST_CASE("URL normalization handles an IPv6 host", "[core][url]") {
    CHECK(normalized("http://[2001:DB8::1]:80") == "http://[2001:db8::1]/");
    CHECK(normalized("http://[2001:DB8::1]:8080/x") == "http://[2001:db8::1]:8080/x");
    CHECK(normalized("http://[::1]/x") == "http://[::1]/x");
}

TEST_CASE("URL normalization keeps user information as typed", "[core][url]") {
    CHECK(normalized("ftp://User@Example.org") == "ftp://User@example.org/");
    CHECK(normalized("https://User:Pa55@Example.org:443/x") == "https://User:Pa55@example.org/x");
    CHECK(normalized("https://a@b@Example.org/") == "https://a@b@example.org/");
}

TEST_CASE("URL normalization changes only the scheme when there is no authority", "[core][url]") {
    CHECK(normalized("MAILTO:A@Example.ORG") == "mailto:A@Example.ORG");
    CHECK(normalized("About:Blank") == "about:Blank");
    CHECK(normalized("file:///Home/A.html") == "file:///Home/A.html");
    CHECK(normalized("FILE://Host/A") == "file://host/A");
    CHECK(normalized("FILE:///x") == "file:///x");
    CHECK(normalized("file://") == "file:///");
}

TEST_CASE("URL normalization is idempotent", "[core][url]") {
    for (const std::string_view url :
         {"HTTPS://Example.ORG:443", "http://[2001:DB8::1]:80", "https://a.org?q#f",
          "ftp://U@A.org:21/P", "about:blank", "http://a.org:/"}) {
        const std::string once = normalized(url);
        CHECK(normalized(once) == once);
    }
}
