#include "cli/record.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "bucu/db/bookmarks.hpp"
#include "cli/escape.hpp"

namespace bucu::cli {

namespace {

constexpr std::string_view kValueIndent = "          ";  // the value column: 10 spaces

// Buku's default colours as SGR codes (docs/product.md section 5, colour). An empty code means no
// colour.
constexpr std::string_view kIdCode = "96";       // bright cyan
constexpr std::string_view kTitleCode = "92;1";  // bold bright green
constexpr std::string_view kLabelCode = "91";    // bright red
constexpr std::string_view kUrlCode = "93";      // bright yellow
constexpr std::string_view kTagsCode = "94";     // bright blue
constexpr std::string_view kFlagsCode = "2";     // dim

// Appends `text`, inside the colour `code` and a reset when there is a code and a text, so a
// coloured part never runs into the next line.
void append_part(std::string& out, std::string_view code, std::string_view text) {
    if (code.empty() || text.empty()) {
        out.append(text);
        return;
    }
    out.append("\x1b[");
    out.append(code);
    out.push_back('m');
    out.append(text);
    out.append("\x1b[0m");
}

// A text and the colour code it takes; an empty code means none.
struct Value {
    std::string_view code;
    std::string_view text;
};

// Appends `value` in its colour, with each line after the first indented to the value column, the
// colour repeated on it with its indent, and an LF after each line.
void append_value(std::string& out, const Value& value) {
    const std::string_view text = value.text;
    std::size_t start = 0;
    bool first = true;
    for (;;) {
        const std::size_t end = text.find('\n', start);
        const std::string_view line =
            text.substr(start, end == std::string_view::npos ? end : end - start);
        append_part(out, value.code,
                    first ? std::string(line) : std::string(kValueIndent).append(line));
        out.push_back('\n');
        if (end == std::string_view::npos) {
            return;
        }
        first = false;
        start = end + 1;
    }
}

constexpr std::size_t kLabelWidth = 7;

// A labelled line's text and the colour of its value.
struct Field {
    std::string_view label;
    Value value;
};

// A labelled line: three spaces, the label padded to kLabelWidth characters, then the value. The
// indent, the label, and its padding take `label_code`.
void append_field(std::string& out, std::string_view label_code, const Field& field) {
    std::string head = "   ";
    head.append(field.label);
    head.append(kLabelWidth - field.label.size(), ' ');
    append_part(out, label_code, head);
    append_value(out, field.value);
}

}  // namespace

std::string format_record(core::BookmarkId id, const core::Bookmark& bookmark, bool colour) {
    // The code of each part: none when the record is plain.
    const auto code = [colour](std::string_view sgr) { return colour ? sgr : std::string_view{}; };
    std::string out;
    if (!bookmark.title.empty()) {
        append_part(out, code(kIdCode), std::to_string(id.value()) + "  ");
        append_value(out, {.code = code(kTitleCode),
                           .text = escape_controls(bookmark.title, LineFeed::kKeep)});
    } else {
        append_part(out, code(kIdCode), std::to_string(id.value()));
        out.push_back('\n');
    }

    append_field(out, code(kLabelCode),
                 {.label = "url:", .value = {.code = code(kUrlCode), .text = bookmark.url}});
    if (!bookmark.description.empty()) {
        append_field(out, code(kLabelCode),
                     {.label = "desc:",
                      .value = {.code = {},
                                .text = escape_controls(bookmark.description, LineFeed::kKeep)}});
    }
    if (!bookmark.tags.empty()) {
        std::string joined;
        for (const std::string& tag : bookmark.tags) {
            joined.append(joined.empty() ? "" : ", ");
            joined.append(tag);
        }
        append_field(out, code(kLabelCode),
                     {.label = "tags:", .value = {.code = code(kTagsCode), .text = joined}});
    }
    if (core::has_flag(bookmark.flags, core::kFlagImmutableTitle)) {
        append_field(
            out, code(kLabelCode),
            {.label = "flags:", .value = {.code = code(kFlagsCode), .text = "immutable-title"}});
    }
    return out;
}

namespace {

// A visitor that writes each record it is given to `out`, with one empty line between records. It
// returns whether the stream is still good, so a failed write ends the walk.
db::BookmarkVisitor record_writer(std::ostream& out, bool colour) {
    return
        [&out, colour, first = true](core::BookmarkId id, const core::Bookmark& bookmark) mutable {
            if (!first) {
                out << '\n';
            }
            first = false;
            out << format_record(id, bookmark, colour);
            return static_cast<bool>(out);
        };
}

}  // namespace

std::expected<void, db::Error> print_records(db::Database& database,
                                             const core::IdSelection& selection, std::ostream& out,
                                             bool colour) {
    return db::for_each_bookmark(database, selection, record_writer(out, colour));
}

std::expected<void, db::Error> print_last_records(db::Database& database, std::int64_t count,
                                                  std::ostream& out, bool colour) {
    return db::for_each_last_bookmark(database, count, record_writer(out, colour));
}

}  // namespace bucu::cli
