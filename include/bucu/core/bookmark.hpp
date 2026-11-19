#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "bucu/core/error.hpp"
#include "bucu/core/tags.hpp"

namespace bucu::core {

// One flag: a bit pattern with the bits of a single flag (product section 4). A raw integer
// becomes a Flag only by an explicit construction.
class Flag {
public:
    explicit constexpr Flag(std::int64_t bits) : bits_(bits) {}
    [[nodiscard]] constexpr std::int64_t bits() const { return bits_; }

private:
    std::int64_t bits_;
};

// A set of flags, stored as the integer Buku uses, bit for bit. Bits BuCu does not know, and the
// highest bit, are kept. A raw integer becomes a FlagSet only by an explicit construction; a Flag
// converts to a FlagSet of that one flag.
class FlagSet {
public:
    constexpr FlagSet() = default;
    explicit constexpr FlagSet(std::int64_t bits) : bits_(bits) {}
    constexpr FlagSet(Flag flag) : bits_(flag.bits()) {}  // NOLINT(google-explicit-constructor)

    // The stored integer.
    [[nodiscard]] constexpr std::int64_t bits() const { return bits_; }

    friend constexpr bool operator==(FlagSet, FlagSet) = default;

private:
    std::int64_t bits_ = 0;
};

// The bit Buku uses, so a database read from Buku keeps its meaning.
inline constexpr Flag kFlagImmutableTitle{1};

// A bookmark without its database ID: the ID belongs to the stored record (#10).
struct Bookmark {
    std::string url;
    std::string title;
    std::string description;
    TagSet tags;
    FlagSet flags;

    friend bool operator==(const Bookmark&, const Bookmark&) = default;
};

// Whether every bit of `flag` is set in `flags`.
bool has_flag(FlagSet flags, Flag flag);

// `flags` with the bits of `flag` set or cleared, and every other bit unchanged.
FlagSet set_flag(FlagSet flags, Flag flag);
FlagSet clear_flag(FlagSet flags, Flag flag);

// A required piece of text. Leaving one out of a designated initializer is ill-formed on every
// compiler, because it has no default constructor; the constructor is explicit so a plain view does
// not convert on its own.
struct RequiredText {
    explicit RequiredText(std::string_view text) : value(text) {}
    std::string_view value;
};

// The text inputs of make_bookmark, named at each call so that none can be swapped
// (Core Guidelines I.4). `url`, `title`, and `description` are required: a designated initializer
// that leaves one out does not build. `tags` is optional; leaving it out
// means no tags. The views are read during the call only.
struct BookmarkFields {
    RequiredText url;
    RequiredText title;
    RequiredText description;
    // The initializer is what makes `tags` optional in a designated initializer: without it, a
    // call that leaves `tags` out fails to build.
    std::string_view tags = {};  // NOLINT(readability-redundant-member-init)
};

// A new bookmark with no flag set. The URL is validated and normalized (product section 4); the
// title and description must be valid UTF-8 and are stored as given. `fields.tags` is the raw tag
// text, parsed with parse_tags, so a Bookmark holds only valid tags. The first broken rule is
// returned, checked in the order URL, title, description, tags.
std::expected<Bookmark, ValidationError> make_bookmark(const BookmarkFields& fields);

}  // namespace bucu::core
