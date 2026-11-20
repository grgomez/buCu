#include "cli/record.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
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

// Appends `value` in the colour `code`, with each line after the first indented to the value
// column, the colour repeated on it with its indent, and an LF after each line.
void append_value(std::string& out, std::string_view code, std::string_view value) {
    std::size_t start = 0;
    bool first = true;
    for (;;) {
        const std::size_t end = value.find('\n', start);
        const std::string_view line =
            value.substr(start, end == std::string_view::npos ? end : end - start);
        append_part(out, code, first ? std::string(line) : std::string(kValueIndent).append(line));
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
    std::string_view value;
    std::string_view value_code;
};

// A labelled line: three spaces, the label padded to kLabelWidth characters, then the value. The
// indent, the label, and its padding take `label_code`.
void append_field(std::string& out, std::string_view label_code, const Field& field) {
    std::string head = "   ";
    head.append(field.label);
    head.append(kLabelWidth - field.label.size(), ' ');
    append_part(out, label_code, head);
    append_value(out, field.value_code, field.value);
}

}  // namespace

std::string format_record(core::BookmarkId id, const core::Bookmark& bookmark, bool colour) {
    // The code of each part: none when the record is plain.
    const auto code = [colour](std::string_view sgr) { return colour ? sgr : std::string_view{}; };
    std::string out;
    if (!bookmark.title.empty()) {
        append_part(out, code(kIdCode), std::to_string(id.value()) + "  ");
        append_value(out, code(kTitleCode), escape_controls(bookmark.title, LineFeed::kKeep));
    } else {
        append_part(out, code(kIdCode), std::to_string(id.value()));
        out.push_back('\n');
    }

    append_field(out, code(kLabelCode),
                 {.label = "url:", .value = bookmark.url, .value_code = code(kUrlCode)});
    if (!bookmark.description.empty()) {
        append_field(out, code(kLabelCode),
                     {.label = "desc:",
                      .value = escape_controls(bookmark.description, LineFeed::kKeep),
                      .value_code = {}});
    }
    if (!bookmark.tags.empty()) {
        std::string joined;
        for (const std::string& tag : bookmark.tags) {
            joined.append(joined.empty() ? "" : ", ");
            joined.append(tag);
        }
        append_field(out, code(kLabelCode),
                     {.label = "tags:", .value = joined, .value_code = code(kTagsCode)});
    }
    if (core::has_flag(bookmark.flags, core::kFlagImmutableTitle)) {
        append_field(
            out, code(kLabelCode),
            {.label = "flags:", .value = "immutable-title", .value_code = code(kFlagsCode)});
    }
    return out;
}

namespace {

// Writes each record it is given to `out`, with one empty line between records.
class RecordWriter {
public:
    RecordWriter(std::ostream& out, bool colour) : out_(out), colour_(colour) {}

    // Returns whether the stream is still good, so a failed write ends the walk.
    bool operator()(core::BookmarkId id, const core::Bookmark& bookmark) {
        if (!first_) {
            out_ << '\n';
        }
        first_ = false;
        out_ << format_record(id, bookmark, colour_);
        return static_cast<bool>(out_);
    }

private:
    std::ostream& out_;
    bool colour_;
    bool first_ = true;
};

}  // namespace

std::expected<void, db::Error> print_records(db::Database& database,
                                             const core::IdSelection& selection, std::ostream& out,
                                             bool colour) {
    RecordWriter writer(out, colour);
    return db::for_each_bookmark(database, selection, std::ref(writer));
}

std::expected<void, db::Error> print_last_records(db::Database& database, std::int64_t count,
                                                  std::ostream& out, bool colour) {
    RecordWriter writer(out, colour);
    return db::for_each_last_bookmark(database, count, std::ref(writer));
}

}  // namespace bucu::cli
