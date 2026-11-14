#include "cli/record.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace bucu::cli {

namespace {

constexpr std::string_view kValueIndent = "          ";  // the value column: 10 spaces

// Appends `value`, with each line after the first indented to the value column, and an LF.
void append_value(std::string& out, std::string_view value) {
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = value.find('\n', start);
        out.append(value.substr(start, end == std::string_view::npos ? end : end - start));
        out.push_back('\n');
        if (end == std::string_view::npos) {
            return;
        }
        out.append(kValueIndent);
        start = end + 1;
    }
}

// A labelled line: three spaces, the label padded to 7 characters, then the value.
void append_field(std::string& out, std::string_view label, std::string_view value) {
    out.append("   ");
    out.append(label);
    out.append(7 - label.size(), ' ');
    append_value(out, value);
}

}  // namespace

std::string format_record(std::int64_t id, const core::Bookmark& bookmark) {
    std::string out = std::to_string(id);
    if (!bookmark.title.empty()) {
        out.append("  ");
        append_value(out, bookmark.title);
    } else {
        out.push_back('\n');
    }

    append_field(out, "url:", bookmark.url);
    if (!bookmark.description.empty()) {
        append_field(out, "desc:", bookmark.description);
    }
    if (!bookmark.tags.empty()) {
        std::string joined;
        for (const std::string& tag : bookmark.tags) {
            joined.append(joined.empty() ? "" : ", ");
            joined.append(tag);
        }
        append_field(out, "tags:", joined);
    }
    if (core::has_flag(bookmark.flags, core::kFlagImmutableTitle)) {
        append_field(out, "flags:", "immutable-title");
    }
    return out;
}

}  // namespace bucu::cli
