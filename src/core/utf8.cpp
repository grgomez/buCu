#include "core/utf8.hpp"

#include <cstddef>

namespace bucu::core {

namespace {

// Decode one code point at `pos`, advancing it. Returns false on malformed input.
bool next_code_point(std::string_view text, std::size_t& pos, char32_t& out) {
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned lead = byte(pos);
    std::size_t length = 0;
    char32_t value = 0;
    char32_t minimum = 0;
    if (lead < 0x80) {
        out = lead;
        ++pos;
        return true;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
        value = lead & 0x1Fu;
        minimum = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        value = lead & 0x0Fu;
        minimum = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        value = lead & 0x07u;
        minimum = 0x10000;
    } else {
        return false;
    }
    if (text.size() - pos < length) {
        return false;
    }
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned cont = byte(pos + i);
        if ((cont & 0xC0u) != 0x80u) {
            return false;
        }
        value = (value << 6) | (cont & 0x3Fu);
    }
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return false;
    }
    out = value;
    pos += length;
    return true;
}

}  // namespace

bool is_valid_utf8(std::string_view text) {
    std::size_t pos = 0;
    char32_t ignored = 0;
    while (pos < text.size()) {
        if (!next_code_point(text, pos, ignored)) {
            return false;
        }
    }
    return true;
}

std::optional<std::u32string> decode_utf8(std::string_view text) {
    std::u32string result;
    std::size_t pos = 0;
    char32_t code_point = 0;
    while (pos < text.size()) {
        if (!next_code_point(text, pos, code_point)) {
            return std::nullopt;
        }
        result.push_back(code_point);
    }
    return result;
}

void append_utf8(std::string& out, char32_t code_point) {
    const auto put = [&out](char32_t value) { out.push_back(static_cast<char>(value)); };
    if (code_point < 0x80) {
        put(code_point);
    } else if (code_point < 0x800) {
        put(0xC0 | (code_point >> 6));
        put(0x80 | (code_point & 0x3F));
    } else if (code_point < 0x10000) {
        put(0xE0 | (code_point >> 12));
        put(0x80 | ((code_point >> 6) & 0x3F));
        put(0x80 | (code_point & 0x3F));
    } else {
        put(0xF0 | (code_point >> 18));
        put(0x80 | ((code_point >> 12) & 0x3F));
        put(0x80 | ((code_point >> 6) & 0x3F));
        put(0x80 | (code_point & 0x3F));
    }
}

}  // namespace bucu::core
