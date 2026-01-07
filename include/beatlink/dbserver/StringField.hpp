#pragma once

#include "Field.hpp"

#include <string>
#include <vector>

namespace beatlink::dbserver {

class DataReader;

class StringField : public Field {
public:
    explicit StringField(DataReader& reader);
    explicit StringField(const std::string& text);

    const std::string& getValue() const { return value_; }

    uint8_t getTypeTag() const override { return typeTag_; }
    uint8_t getArgumentTag() const override { return 0x02; }
    size_t getSize() const override { return size_; }
    std::span<const uint8_t> getBytes() const override { return buffer_; }
    std::string toString() const override;

private:
    static std::string decodeUtf16Be(const uint8_t* data, size_t length);
    static std::vector<uint8_t> encodeUtf16Be(const std::string& text);

    uint8_t typeTag_ = 0x26;
    size_t size_ = 0;
    std::string value_;
    std::vector<uint8_t> buffer_;
};

} // namespace beatlink::dbserver
