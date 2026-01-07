#include "beatlink/dbserver/StringField.hpp"
#include "beatlink/dbserver/DataReader.hpp"
#include "beatlink/Util.hpp"

#include <codecvt>
#include <locale>
#include <stdexcept>

namespace beatlink::dbserver {

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
        value_ = decodeUtf16Be(buffer_.data() + 5, size_ - 2);
    } else {
        value_.clear();
    }
}

StringField::StringField(const std::string& text)
    : value_(text)
{
    auto utf16 = encodeUtf16Be(text + '\0');
    size_ = utf16.size();
    buffer_.resize(size_ + 5);
    buffer_[0] = typeTag_;

    const int charCount = static_cast<int>(size_ / 2);
    beatlink::Util::numberToBytes(charCount, buffer_.data(), buffer_.size(), 1, 4);
    std::copy(utf16.begin(), utf16.end(), buffer_.begin() + 5);
}

std::string StringField::decodeUtf16Be(const uint8_t* data, size_t length) {
    if (length == 0) {
        return {};
    }
    if (length % 2 != 0) {
        throw std::runtime_error("Invalid UTF-16BE string length");
    }
    std::u16string utf16;
    utf16.resize(length / 2);
    for (size_t i = 0; i < utf16.size(); ++i) {
        utf16[i] = static_cast<char16_t>((data[i * 2] << 8) | data[i * 2 + 1]);
    }

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(utf16);
}

std::vector<uint8_t> StringField::encodeUtf16Be(const std::string& text) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    std::u16string utf16 = converter.from_bytes(text);
    std::vector<uint8_t> bytes;
    bytes.resize(utf16.size() * 2);
    for (size_t i = 0; i < utf16.size(); ++i) {
        bytes[i * 2] = static_cast<uint8_t>((utf16[i] >> 8) & 0xff);
        bytes[i * 2 + 1] = static_cast<uint8_t>(utf16[i] & 0xff);
    }
    return bytes;
}

std::string StringField::toString() const {
    return "StringField[ size: " + std::to_string(size_) + ", value: \"" + value_ + "\", bytes: " +
           getHexString() + "]";
}

} // namespace beatlink::dbserver
