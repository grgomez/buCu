#include "bucu/core/url.hpp"

#include <cstddef>
#include <string>
#include <string_view>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

namespace bucu::core {

namespace {

ValidationError refuse(Reason reason, std::string_view url) {
    return ValidationError{Field::kUrl, reason, std::string(url)};
}

// `code_points` without its leading and trailing White_Space.
std::u32string_view trim(std::u32string_view code_points) {
    while (!code_points.empty() && is_white_space(code_points.front())) {
        code_points.remove_prefix(1);
    }
    while (!code_points.empty() && is_white_space(code_points.back())) {
        code_points.remove_suffix(1);
    }
    return code_points;
}

bool is_ascii_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_ascii_digit(char c) { return c >= '0' && c <= '9'; }

// FR-001: the text after the first ':' starts with digits and then ends or has '/' (a host and
// port, as in Buku), or the part before it is one letter (a drive letter).
bool lacks_scheme(std::string_view url, std::size_t colon) {
    if (colon == 1 && is_ascii_alpha(url[0])) {
        return true;
    }
    std::size_t i = colon + 1;
    while (i < url.size() && is_ascii_digit(url[i])) {
        ++i;
    }
    return i > colon + 1 && (i == url.size() || url[i] == '/');
}

// RFC 3986 scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ).
bool is_scheme(std::string_view text) {
    if (text.empty() || !is_ascii_alpha(text[0])) {
        return false;
    }
    for (const char c : text) {
        if (!is_ascii_alpha(c) && !is_ascii_digit(c) && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

bool equals_ignoring_ascii_case(std::string_view text, std::string_view lower) {
    if (text.size() != lower.size()) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const char folded = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        if (folded != lower[i]) {
            return false;
        }
    }
    return true;
}

// The parts of a URL that passed the scheme test. The views point into the trimmed URL.
struct Parts {
    std::string_view scheme;
    bool has_authority = false;
    std::string_view user_info;  // with the trailing '@', or empty
    std::string_view host;
    std::string_view port;  // without ':', or empty
    std::string_view rest;  // path, query, and fragment as typed
};

// D6: the authority ends at the first '/', '?', or '#'; user information is before the last
// '@'; the port is after the last ':' outside brackets.
Parts split(std::string_view url, std::size_t colon) {
    Parts parts;
    parts.scheme = url.substr(0, colon);
    std::string_view after = url.substr(colon + 1);
    if (!after.starts_with("//")) {
        parts.rest = after;
        return parts;
    }
    parts.has_authority = true;
    after.remove_prefix(2);
    const std::size_t end = after.find_first_of("/?#");
    std::string_view authority = after.substr(0, end);
    parts.rest = end == std::string_view::npos ? std::string_view() : after.substr(end);
    const std::size_t at = authority.rfind('@');
    if (at != std::string_view::npos) {
        parts.user_info = authority.substr(0, at + 1);
        authority.remove_prefix(at + 1);
    }
    const std::size_t close = authority.rfind(']');
    const std::size_t port_colon = authority.rfind(':');
    if (port_colon != std::string_view::npos &&
        (close == std::string_view::npos || port_colon > close)) {
        parts.port = authority.substr(port_colon + 1);
        authority = authority.substr(0, port_colon);
    }
    parts.host = authority;
    return parts;
}

}  // namespace

std::expected<std::string, ValidationError> normalize_url(std::string_view input) {
    const auto decoded = decode_utf8(input);
    if (!decoded) {
        return std::unexpected(refuse(Reason::kInvalidUtf8, input));
    }
    const std::u32string_view trimmed = trim(*decoded);
    if (trimmed.empty()) {
        return std::unexpected(refuse(Reason::kEmpty, input));
    }
    std::string url;
    for (const char32_t cp : trimmed) {
        if (is_white_space(cp)) {
            return std::unexpected(refuse(Reason::kWhitespace, input));
        }
        if (is_control(cp)) {
            return std::unexpected(refuse(Reason::kControlCharacter, input));
        }
        append_utf8(url, cp);
    }
    const std::size_t colon = url.find(':');
    if (colon == std::string_view::npos || lacks_scheme(url, colon) ||
        !is_scheme(url.substr(0, colon)) || colon + 1 == url.size()) {
        return std::unexpected(refuse(Reason::kNoScheme, input));
    }
    const Parts parts = split(url, colon);
    if (parts.has_authority && parts.host.empty() &&
        !equals_ignoring_ascii_case(parts.scheme, "file")) {
        return std::unexpected(refuse(Reason::kEmptyHost, input));
    }
    return url;
}

}  // namespace bucu::core
