#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bucu/core/url.hpp"
#include "catch_amalgamated.hpp"
// rapidcheck/catch.h needs Catch2's header first.
#include <rapidcheck/catch.h>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

namespace {

using bucu::core::append_utf8;
using bucu::core::is_control;
using bucu::core::is_white_space;
using bucu::core::normalize_url;

// How long the generated pieces get (the generator size, not a rule).
constexpr int kSchemeTailSize = 6;
constexpr int kHostNameSize = 12;
constexpr int kHostUnicodeSize = 6;
constexpr int kIpv6Size = 20;
constexpr int kPortSize = 5;
constexpr int kPathPartSize = 10;
constexpr int kUserInfoSize = 8;
constexpr int kOpaqueRestSize = 12;
constexpr int kPieceCount = 12;

// Non-ASCII code point ranges a host may hold.
constexpr std::uint32_t kFirstAfterC1Controls = 0xA1;
constexpr std::uint32_t kLastLatinExtendedB = 0x24F;
constexpr std::uint32_t kFirstGreek = 0x370;
constexpr std::uint32_t kLastGreek = 0x3FF;
constexpr std::uint32_t kFirstCyrillic = 0x400;
constexpr std::uint32_t kLastCyrillic = 0x4FF;
constexpr std::uint32_t kFirstCjkSample = 0x4E00;
constexpr std::uint32_t kLastCjkSample = 0x4FFF;
constexpr char32_t kDottedCapitalI = 0x0130;  // lowercases to two code points

std::string join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) {
        out += part;
    }
    return out;
}

// A string of one to `max` characters (the generator size), each drawn from `alphabet`.
rc::Gen<std::string> from_alphabet(const std::string& alphabet, int max) {
    return rc::gen::resize(
        max, rc::gen::nonEmpty(rc::gen::container<std::string>(rc::gen::elementOf(alphabet))));
}

rc::Gen<std::string> optional_text(const rc::Gen<std::string>& text) {
    return rc::gen::oneOf(rc::gen::just(std::string()), text);
}

// An RFC 3986 scheme of two or more characters, in any case. A one-letter scheme would be a
// drive letter (product section 4 URL).
rc::Gen<std::string> scheme() {
    const std::string letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    return rc::gen::map(
        rc::gen::tuple(rc::gen::elementOf(letters), rc::gen::elementOf(letters),
                       optional_text(from_alphabet(letters + "0123456789+-.", kSchemeTailSize))),
        [](const auto& parts) {
            return std::string(1, std::get<0>(parts)) + std::get<1>(parts) + std::get<2>(parts);
        });
}

rc::Gen<char32_t> host_code_point() {
    const auto in = [](std::uint32_t first, std::uint32_t last) {
        return rc::gen::map(rc::gen::inRange<std::uint32_t>(first, last + 1),
                            [](std::uint32_t v) { return static_cast<char32_t>(v); });
    };
    const auto non_ascii =
        rc::gen::oneOf(in(kFirstAfterC1Controls, kLastLatinExtendedB), in(kFirstGreek, kLastGreek),
                       in(kFirstCyrillic, kLastCyrillic), rc::gen::just(kDottedCapitalI),
                       in(kFirstCjkSample, kLastCjkSample));
    return rc::gen::suchThat(non_ascii,
                             [](char32_t cp) { return !is_white_space(cp) && !is_control(cp); });
}

// A host: a name in any case, with non-ASCII letters, or an IPv6 address in brackets.
rc::Gen<std::string> host() {
    const auto name = rc::gen::oneOf(
        from_alphabet("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-",
                      kHostNameSize),
        rc::gen::map(rc::gen::resize(kHostUnicodeSize,
                                     rc::gen::nonEmpty(rc::gen::container<std::vector<char32_t>>(
                                         host_code_point()))),
                     [](const auto& code_points) {
                         std::string out;
                         for (const char32_t cp : code_points) {
                             append_utf8(out, cp);
                         }
                         return out;
                     }));
    const auto ipv6 = rc::gen::map(from_alphabet("0123456789abcdefABCDEF:", kIpv6Size),
                                   [](const std::string& digits) { return "[" + digits + "]"; });
    return rc::gen::oneOf(name, ipv6);
}

// The part after the host: a port, which may be a default one or malformed, then a path, a query,
// and a fragment, each optional.
rc::Gen<std::string> authority_tail() {
    const std::string safe = "abcXYZ019-._~%/";
    return rc::gen::map(
        rc::gen::tuple(optional_text(rc::gen::map(
                           rc::gen::oneOf(from_alphabet("0123456789", kPortSize),
                                          rc::gen::element(std::string("80"), std::string("443"),
                                                           std::string("21"), std::string("abc"))),
                           [](const std::string& port) { return ":" + port; })),
                       optional_text(rc::gen::map(from_alphabet(safe, kPathPartSize),
                                                  [](const std::string& s) { return "/" + s; })),
                       optional_text(rc::gen::map(from_alphabet(safe + "=&", kPathPartSize),
                                                  [](const std::string& s) { return "?" + s; })),
                       optional_text(rc::gen::map(from_alphabet(safe, kPathPartSize),
                                                  [](const std::string& s) { return "#" + s; }))),
        [](const auto& parts) {
            return std::get<0>(parts) + std::get<1>(parts) + std::get<2>(parts) +
                   std::get<3>(parts);
        });
}

// A URL with an authority, or without one, as a browser export may hold it.
rc::Gen<std::string> generated_url() {
    const auto with_authority = rc::gen::map(
        rc::gen::tuple(scheme(),
                       optional_text(rc::gen::map(from_alphabet("abcXYZ019:._~%", kUserInfoSize),
                                                  [](const std::string& s) { return s + "@"; })),
                       host(), authority_tail()),
        [](const auto& parts) {
            return std::get<0>(parts) + "://" + std::get<1>(parts) + std::get<2>(parts) +
                   std::get<3>(parts);
        });
    // The rest starts with a letter, so it is neither "//" (an authority) nor digits (a port).
    const auto without_authority = rc::gen::map(
        rc::gen::tuple(scheme(), rc::gen::elementOf(std::string("abcXYZ")),
                       optional_text(from_alphabet("abcXYZ019@._~%/?#=&-", kOpaqueRestSize))),
        [](const auto& parts) {
            return std::get<0>(parts) + ":" + std::get<1>(parts) + std::get<2>(parts);
        });
    return rc::gen::oneOf(with_authority, with_authority, without_authority);
}

// Text that is close to a URL but often is not one: the characters that give a URL its structure,
// white space, a control character, and non-ASCII letters, in any order.
rc::Gen<std::string> arbitrary_url_text() {
    const std::vector<std::string> pieces = {
        "a",        "Z",        "9",        ":",        "/",           "//",  "?",  "#",
        "@",        "[",        "]",        ".",        "-",           " ",   "\t", "\n",
        "http",     "FILE",     "https",    "ftp",      "80",          "443", "21", "\x01",
        "\xC3\x89", "\xCE\xA3", "\xC4\xB0", "\xC2\xA0", "\xE3\x80\x80"};
    return rc::gen::map(rc::gen::resize(kPieceCount, rc::gen::container<std::vector<std::string>>(
                                                         rc::gen::elementOf(pieces))),
                        join);
}

// White space around a URL, which normalization trims.
rc::Gen<std::string> padding() {
    const std::vector<std::string> pieces = {" ", "\t", "\n", "\r", "\xC2\xA0", "\xE3\x80\x80"};
    return rc::gen::map(rc::gen::container<std::vector<std::string>>(rc::gen::elementOf(pieces)),
                        join);
}

}  // namespace

// Product section 4 URL.
TEST_CASE(
    "property: a generated URL normalizes, and the result is accepted and stable (product 4 URL)",
    "[core][url][property]") {
    rc::prop("normalizing a well-formed URL succeeds and its result normalizes to itself", [] {
        const std::string url = *generated_url();
        const auto once = normalize_url(url);
        RC_ASSERT(once.has_value());
        const auto twice = normalize_url(*once);
        RC_ASSERT(twice.has_value());
        RC_ASSERT(*twice == *once);
    });
}

TEST_CASE("property: whatever normalization accepts it accepts again (product 4 URL)",
          "[core][url][property]") {
    rc::prop("a normalized URL is accepted again and normalizes to itself", [] {
        const std::string text = *rc::gen::oneOf(arbitrary_url_text(), generated_url());
        const auto once = normalize_url(text);
        if (once) {
            const auto twice = normalize_url(*once);
            RC_ASSERT(twice.has_value());
            RC_ASSERT(*twice == *once);
        }
    });
}

TEST_CASE("property: white space around a URL does not change the result (product 4 URL)",
          "[core][url][property]") {
    rc::prop("the trimmed and the padded URL normalize alike", [] {
        const std::string text = *rc::gen::oneOf(arbitrary_url_text(), generated_url());
        const auto plain = normalize_url(text);
        const auto padded = normalize_url(*padding() + text + *padding());
        RC_ASSERT(padded.has_value() == plain.has_value());
        if (plain) {
            RC_ASSERT(*padded == *plain);
        }
    });
}
