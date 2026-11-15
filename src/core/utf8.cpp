#include "core/utf8.hpp"

#include <cstddef>
#include <optional>

namespace bucu::core {

namespace {

// UTF-8 structure (RFC 3629): the lead byte's high bits give the length, and each continuation
// byte is 10xxxxxx and carries six bits.
constexpr unsigned kAsciiLimit = 0x80;
constexpr unsigned kLead2Min = 0xC2;  // 0xC0 and 0xC1 would encode an overlong form
constexpr unsigned kLead2Max = 0xDF;
constexpr unsigned kLead3Min = 0xE0;
constexpr unsigned kLead3Max = 0xEF;
constexpr unsigned kLead4Min = 0xF0;
constexpr unsigned kLead4Max = 0xF4;  // above 0xF4 would pass U+10FFFF
constexpr unsigned kLead2Payload = 0x1F;
constexpr unsigned kLead3Payload = 0x0F;
constexpr unsigned kLead4Payload = 0x07;
constexpr unsigned kContinuationMask = 0xC0;
constexpr unsigned kContinuationTag = 0x80;
constexpr unsigned kContinuationPayload = 0x3F;
constexpr unsigned kContinuationBits = 6;
constexpr std::size_t kLength2 = 2;
constexpr std::size_t kLength3 = 3;
constexpr std::size_t kLength4 = 4;
constexpr char32_t kMin2Byte = 0x80;  // the smallest code point that needs each length
constexpr char32_t kMin3Byte = 0x800;
constexpr char32_t kMin4Byte = 0x10000;
constexpr char32_t kMaxCodePoint = 0x10FFFF;
constexpr char32_t kSurrogateMin = 0xD800;
constexpr char32_t kSurrogateMax = 0xDFFF;
constexpr char32_t kLead2Tag = 0xC0;  // the lead byte's tag bits when writing
constexpr char32_t kLead3Tag = 0xE0;
constexpr char32_t kLead4Tag = 0xF0;
constexpr unsigned kShift2 = 6;  // the shifts that bring each six-bit group down
constexpr unsigned kShift3 = 12;
constexpr unsigned kShift4 = 18;

// Decode one code point at `pos`, advancing it. Returns nothing on malformed input.
std::optional<char32_t> next_code_point(std::string_view text, std::size_t& pos) {
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned lead = byte(pos);
    std::size_t length = 0;
    char32_t value = 0;
    char32_t minimum = 0;
    if (lead < kAsciiLimit) {
        ++pos;
        return lead;
    }
    if (lead >= kLead2Min && lead <= kLead2Max) {
        length = kLength2;
        value = lead & kLead2Payload;
        minimum = kMin2Byte;
    } else if (lead >= kLead3Min && lead <= kLead3Max) {
        length = kLength3;
        value = lead & kLead3Payload;
        minimum = kMin3Byte;
    } else if (lead >= kLead4Min && lead <= kLead4Max) {
        length = kLength4;
        value = lead & kLead4Payload;
        minimum = kMin4Byte;
    } else {
        return std::nullopt;
    }
    if (text.size() - pos < length) {
        return std::nullopt;
    }
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned cont = byte(pos + i);
        if ((cont & kContinuationMask) != kContinuationTag) {
            return std::nullopt;
        }
        value = (value << kContinuationBits) | (cont & kContinuationPayload);
    }
    if (value < minimum || value > kMaxCodePoint ||
        (value >= kSurrogateMin && value <= kSurrogateMax)) {
        return std::nullopt;
    }
    pos += length;
    return value;
}

}  // namespace

bool is_valid_utf8(std::string_view text) {
    std::size_t pos = 0;
    while (pos < text.size()) {
        if (!next_code_point(text, pos)) {
            return false;
        }
    }
    return true;
}

std::optional<std::u32string> decode_utf8(std::string_view text) {
    std::u32string result;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::optional<char32_t> code_point = next_code_point(text, pos);
        if (!code_point) {
            return std::nullopt;
        }
        result.push_back(*code_point);
    }
    return result;
}

void append_utf8(std::string& out, char32_t code_point) {
    const auto put = [&out](char32_t value) { out.push_back(static_cast<char>(value)); };
    const auto continuation = [code_point](unsigned shift) {
        return kContinuationTag | ((code_point >> shift) & kContinuationPayload);
    };
    if (code_point < kMin2Byte) {
        put(code_point);
    } else if (code_point < kMin3Byte) {
        put(kLead2Tag | (code_point >> kShift2));
        put(continuation(0));
    } else if (code_point < kMin4Byte) {
        put(kLead3Tag | (code_point >> kShift3));
        put(continuation(kShift2));
        put(continuation(0));
    } else {
        put(kLead4Tag | (code_point >> kShift4));
        put(continuation(kShift3));
        put(continuation(kShift2));
        put(continuation(0));
    }
}

}  // namespace bucu::core
