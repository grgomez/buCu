#include "bucu/core/changes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "bucu/core/bookmark.hpp"
#include "catch_amalgamated.hpp"

namespace {

using bucu::core::apply_changes;
using bucu::core::Bookmark;
using bucu::core::BookmarkChanges;
using bucu::core::ChangeRequest;
using bucu::core::Field;
using bucu::core::FlagChange;
using bucu::core::FlagSet;
using bucu::core::make_changes;
using bucu::core::Reason;
using bucu::core::TagChange;
using bucu::core::TagOperation;
using bucu::core::TagSet;

// Flag bit 1 (immutable title) and bit 4, which BuCu does not know.
constexpr std::int64_t kFlagsOneAndFour = 5;

Bookmark stored() {
    return Bookmark{.url = "https://example.org/",
                    .title = "Title",
                    .description = "Description",
                    .tags = {"a", "b"},
                    .flags = FlagSet(kFlagsOneAndFour)};
}

BookmarkChanges checked(const ChangeRequest& request) {
    const auto changes = make_changes(request);
    REQUIRE(changes.has_value());
    return *changes;
}

Bookmark applied(const ChangeRequest& request) { return apply_changes(stored(), checked(request)); }

Bookmark with_flags(std::int64_t bits) {
    return Bookmark{.url = "", .title = "", .description = "", .tags = {}, .flags = FlagSet(bits)};
}

ChangeRequest tag_request(TagOperation operation, std::string text) {
    return ChangeRequest{.tags = TagChange{.operation = operation, .text = std::move(text)}};
}

}  // namespace

TEST_CASE("Bookmark equality compares every field", "[core][changes]") {
    CHECK(stored() == stored());
    for (auto change :
         {+[](Bookmark& b) { b.url += "x"; }, +[](Bookmark& b) { b.title += "x"; },
          +[](Bookmark& b) { b.description += "x"; }, +[](Bookmark& b) { b.tags.insert("c"); },
          +[](Bookmark& b) { b.flags = FlagSet(0); }}) {
        Bookmark other = stored();
        change(other);
        CHECK_FALSE(other == stored());
    }
}

TEST_CASE("an empty request changes nothing", "[core][changes]") {
    CHECK(applied(ChangeRequest{}) == stored());
}

TEST_CASE("the URL is normalized, and the other fields stay", "[core][changes]") {
    const Bookmark result = applied(ChangeRequest{.url = "HTTPS://Example.ORG:443/x"});

    CHECK(result.url == "https://example.org/x");
    CHECK(result.title == "Title");
    CHECK(result.description == "Description");
    CHECK(result.tags == TagSet{"a", "b"});
    CHECK(result.flags == FlagSet(kFlagsOneAndFour));
}

TEST_CASE("a title and a description replace the stored ones, and empty clears them",
          "[core][changes]") {
    const Bookmark replaced = applied(ChangeRequest{.title = "New", .description = "Words"});
    const Bookmark cleared = applied(ChangeRequest{.title = "", .description = ""});

    CHECK(replaced.title == "New");
    CHECK(replaced.description == "Words");
    CHECK(cleared.title.empty());
    CHECK(cleared.description.empty());
    CHECK(cleared.url == "https://example.org/");
}

TEST_CASE("add is the union of the tags, normalized as in product section 4", "[core][changes]") {
    CHECK(applied(tag_request(TagOperation::kAdd, "c, D")).tags == TagSet{"a", "b", "c", "d"});
    CHECK(applied(tag_request(TagOperation::kAdd, "A")).tags == TagSet{"a", "b"});
    CHECK(applied(tag_request(TagOperation::kAdd, " , ")).tags == TagSet{"a", "b"});
}

TEST_CASE("remove takes tags away and ignores a tag the bookmark lacks", "[core][changes]") {
    CHECK(applied(tag_request(TagOperation::kRemove, "b, zz")).tags == TagSet{"a"});
    CHECK(applied(tag_request(TagOperation::kRemove, "A,B")).tags.empty());
    CHECK(applied(tag_request(TagOperation::kRemove, "zz")).tags == TagSet{"a", "b"});
}

TEST_CASE("replace sets the tags, and empty text removes them all", "[core][changes]") {
    CHECK(applied(tag_request(TagOperation::kReplace, "q, Q, r")).tags == TagSet{"q", "r"});
    CHECK(applied(tag_request(TagOperation::kReplace, "")).tags.empty());
    CHECK(applied(tag_request(TagOperation::kReplace, ",")).tags.empty());
}

TEST_CASE("setting the flag keeps the other bits, and clearing it keeps bit 4", "[core][changes]") {
    const Bookmark unset =
        apply_changes(with_flags(4), checked(ChangeRequest{.flag = FlagChange::kSet}));
    const Bookmark cleared = applied(ChangeRequest{.flag = FlagChange::kClear});
    const Bookmark unknown_bits =
        apply_changes(with_flags(-1), checked(ChangeRequest{.flag = FlagChange::kClear}));

    CHECK(unset.flags == FlagSet(kFlagsOneAndFour));
    CHECK(cleared.flags == FlagSet(4));
    CHECK(unknown_bits.flags == FlagSet(-2));
}

TEST_CASE("setting a flag that is set, or clearing one that is clear, changes nothing",
          "[core][changes]") {
    CHECK(applied(ChangeRequest{.flag = FlagChange::kSet}) == stored());
    const Bookmark no_flag =
        apply_changes(with_flags(4), checked(ChangeRequest{.flag = FlagChange::kClear}));
    CHECK(no_flag.flags == FlagSet(4));
}

TEST_CASE("every field can change in one request", "[core][changes]") {
    const Bookmark result =
        applied(ChangeRequest{.url = "https://new.example",
                              .title = "T",
                              .description = "D",
                              .tags = TagChange{.operation = TagOperation::kReplace, .text = "x"},
                              .flag = FlagChange::kClear});

    CHECK(result == Bookmark{.url = "https://new.example/",
                             .title = "T",
                             .description = "D",
                             .tags = {"x"},
                             .flags = FlagSet(4)});
}

TEST_CASE("a bad URL is refused with the URL rule", "[core][changes]") {
    const auto changes = make_changes(ChangeRequest{.url = "example.org"});

    REQUIRE_FALSE(changes.has_value());
    CHECK(changes.error().field == Field::kUrl);
    CHECK(changes.error().reason == Reason::kNoScheme);
    CHECK(changes.error().value == "example.org");
}

TEST_CASE("a title or description that is not valid UTF-8 is refused", "[core][changes]") {
    const auto title = make_changes(ChangeRequest{.title = "a\xff"});
    const auto description = make_changes(ChangeRequest{.description = "\xc3"});

    REQUIRE_FALSE(title.has_value());
    CHECK(title.error().field == Field::kTitle);
    CHECK(title.error().reason == Reason::kInvalidUtf8);
    REQUIRE_FALSE(description.has_value());
    CHECK(description.error().field == Field::kDescription);
    CHECK(description.error().reason == Reason::kInvalidUtf8);
}

TEST_CASE("a bad tag is refused in every operation", "[core][changes]") {
    for (const TagOperation operation :
         {TagOperation::kAdd, TagOperation::kRemove, TagOperation::kReplace}) {
        const auto changes = make_changes(tag_request(operation, "ok, a\x01"));

        REQUIRE_FALSE(changes.has_value());
        CHECK(changes.error().field == Field::kTag);
        CHECK(changes.error().reason == Reason::kControlCharacter);
    }
}

TEST_CASE("the first broken rule is returned, in the order URL, title, description, tags",
          "[core][changes]") {
    const auto all = make_changes(
        ChangeRequest{.url = "bad",
                      .title = "\xff",
                      .description = "\xff",
                      .tags = TagChange{.operation = TagOperation::kAdd, .text = "\xff"}});
    const auto no_url = make_changes(
        ChangeRequest{.url = "https://a.example",
                      .title = "\xff",
                      .description = "\xff",
                      .tags = TagChange{.operation = TagOperation::kAdd, .text = "\xff"}});
    const auto no_title = make_changes(ChangeRequest{
        .description = "\xff", .tags = TagChange{.operation = TagOperation::kAdd, .text = "\xff"}});
    const auto only_tag = make_changes(
        ChangeRequest{.tags = TagChange{.operation = TagOperation::kAdd, .text = "\xff"}});

    REQUIRE_FALSE(all.has_value());
    CHECK(all.error().field == Field::kUrl);
    REQUIRE_FALSE(no_url.has_value());
    CHECK(no_url.error().field == Field::kTitle);
    REQUIRE_FALSE(no_title.has_value());
    CHECK(no_title.error().field == Field::kDescription);
    REQUIRE_FALSE(only_tag.has_value());
    CHECK(only_tag.error().field == Field::kTag);
}
