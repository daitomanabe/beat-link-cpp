#include "beatlink/dbserver/StringField.hpp"
#include "beatlink/dbserver/DataReader.hpp"
#include "beatlink/Util.hpp"

#include <stdexcept>

namespace beatlink::dbserver {

namespace {

// UTF-16 BE to UTF-8 conversion without deprecated codecvt
std::string utf16beToUtf8(const uint8_t* data, size_t length) {
    if (length == 0 || length % 2 != 0) {
        return {};
    }

    std::string result;
    result.reserve(length);  // Rough estimate

    for (size_t i = 0; i < length; i += 2) {
        // Read UTF-16 BE code unit
        uint32_t codeUnit = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];

        // Handle surrogate pairs
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF && i + 2 < length) {
            uint32_t highSurrogate = codeUnit;
            uint32_t lowSurrogate = (static_cast<uint32_t>(data[i + 2]) << 8) | data[i + 3];
            if (lowSurrogate >= 0xDC00 && lowSurrogate <= 0xDFFF) {
                // Valid surrogate pair - decode to code point
                uint32_t codePoint = 0x10000 + ((highSurrogate - 0xD800) << 10) + (lowSurrogate - 0xDC00);
                i += 2;  // Skip low surrogate

                // Encode as UTF-8 (4 bytes)
                result.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
                result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                continue;
            }
        }

        // Not a surrogate or invalid surrogate - treat as BMP character
        if (codeUnit < 0x80) {
            // ASCII (1 byte)
            result.push_back(static_cast<char>(codeUnit));
        } else if (codeUnit < 0x800) {
            // 2-byte UTF-8
            result.push_back(static_cast<char>(0xC0 | ((codeUnit >> 6) & 0x1F)));
            result.push_back(static_cast<char>(0x80 | (codeUnit & 0x3F)));
        } else {
            // 3-byte UTF-8
            result.push_back(static_cast<char>(0xE0 | ((codeUnit >> 12) & 0x0F)));
            result.push_back(static_cast<char>(0x80 | ((codeUnit >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codeUnit & 0x3F)));
        }
    }

    return result;
}

// UTF-8 to UTF-16 BE conversion without deprecated codecvt
std::vector<uint8_t> utf8ToUtf16be(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 2);

    size_t i = 0;
    while (i < text.size()) {
        uint32_t codePoint = 0;
        uint8_t byte = static_cast<uint8_t>(text[i]);

        if ((byte & 0x80) == 0) {
            // ASCII (1 byte)
            codePoint = byte;
            i += 1;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte UTF-8
            if (i + 1 >= text.size()) break;
            codePoint = ((byte & 0x1F) << 6) |
                        (static_cast<uint8_t>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte UTF-8
            if (i + 2 >= text.size()) break;
            codePoint = ((byte & 0x0F) << 12) |
                        ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte UTF-8 (supplementary plane - needs surrogate pair)
            if (i + 3 >= text.size()) break;
            codePoint = ((byte & 0x07) << 18) |
                        ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 12) |
                        ((static_cast<uint8_t>(text[i + 2]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(text[i + 3]) & 0x3F);
            i += 4;

            // Encode as surrogate pair
            codePoint -= 0x10000;
            uint16_t highSurrogate = 0xD800 + static_cast<uint16_t>((codePoint >> 10) & 0x3FF);
            uint16_t lowSurrogate = 0xDC00 + static_cast<uint16_t>(codePoint & 0x3FF);

            result.push_back(static_cast<uint8_t>((highSurrogate >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(highSurrogate & 0xFF));
            result.push_back(static_cast<uint8_t>((lowSurrogate >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(lowSurrogate & 0xFF));
            continue;
        } else {
            // Invalid UTF-8 byte - skip
            i += 1;
            continue;
        }

        // BMP character - single UTF-16 code unit
        result.push_back(static_cast<uint8_t>((codePoint >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(codePoint & 0xFF));
    }

    return result;
}

} // namespace

StringField::StringField(DataReader& reader) {
    uint8_t sizeBytes[4] = {};
    reader.readFully(sizeBytes, sizeof(sizeBytes));
    size_ = static_cast<size_t>(beatlink::Util::bytesToNumber(sizeBytes, 0, 4)) * 2;

    buffer_.resize(size_ + 5);
    buffer_[0] = typeTag_;
    std::copy(sizeBytes, sizeBytes + 4, buffer_.begin() + 1);
    if (size_ > 0) {
        reader.readFully(buffer_.data() + 5, size_);
    }

    if (size_ >= 2) {
        value_ = utf16beToUtf8(buffer_.data() + 5, size_ - 2);
    } else {
        value_.clear();
    }
}

StringField::StringField(const std::string& text)
    : value_(text)
{
    auto utf16 = utf8ToUtf16be(text + '\0');
    size_ = utf16.size();
    buffer_.resize(size_ + 5);
    buffer_[0] = typeTag_;

    const int charCount = static_cast<int>(size_ / 2);
    beatlink::Util::numberToBytes(charCount, buffer_.data(), buffer_.size(), 1, 4);
    std::copy(utf16.begin(), utf16.end(), buffer_.begin() + 5);
}

std::string StringField::decodeUtf16Be(const uint8_t* data, size_t length) {
    return utf16beToUtf8(data, length);
}

std::vector<uint8_t> StringField::encodeUtf16Be(const std::string& text) {
    return utf8ToUtf16be(text);
}

std::string StringField::toString() const {
    return "StringField[ size: " + std::to_string(size_) + ", value: \"" + value_ + "\", bytes: " +
           getHexString() + "]";
}

} // namespace beatlink::dbserver
