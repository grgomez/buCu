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

constexpr std::size_t kLabelWidth = 7;

// A labelled line: three spaces, the label padded to kLabelWidth characters, then the value.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the label is always a literal
void append_field(std::string& out, std::string_view label, std::string_view value) {
    out.append("   ");
    out.append(label);
    out.append(kLabelWidth - label.size(), ' ');
    append_value(out, value);
}

}  // namespace

std::string format_record(core::BookmarkId id, const core::Bookmark& bookmark) {
    std::string out = std::to_string(id.value());
    if (!bookmark.title.empty()) {
        out.append("  ");
        append_value(out, escape_controls(bookmark.title, LineFeed::kKeep));
    } else {
        out.push_back('\n');
    }

    append_field(out, "url:", bookmark.url);
    if (!bookmark.description.empty()) {
        append_field(out, "desc:", escape_controls(bookmark.description, LineFeed::kKeep));
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

namespace {

// Writes each record it is given to `out`, with one empty line between records.
class RecordWriter {
public:
    explicit RecordWriter(std::ostream& out) : out_(out) {}

    // Returns whether the stream is still good, so a failed write ends the walk.
    bool operator()(core::BookmarkId id, const core::Bookmark& bookmark) {
        if (!first_) {
            out_ << '\n';
        }
        first_ = false;
        out_ << format_record(id, bookmark);
        return static_cast<bool>(out_);
    }

private:
    std::ostream& out_;
    bool first_ = true;
};

}  // namespace

std::expected<void, db::Error> print_records(db::Database& database,
                                             const core::IdSelection& selection,
                                             std::ostream& out) {
    RecordWriter writer(out);
    return db::for_each_bookmark(database, selection, std::ref(writer));
}

std::expected<void, db::Error> print_last_records(db::Database& database, std::int64_t count,
                                                  std::ostream& out) {
    RecordWriter writer(out);
    return db::for_each_last_bookmark(database, count, std::ref(writer));
}

}  // namespace bucu::cli
