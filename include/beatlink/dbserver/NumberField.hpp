#pragma once

#include "Field.hpp"
#include "beatlink/Util.hpp"

#include <cstdint>
#include <vector>

namespace beatlink::dbserver {

class DataReader;

class NumberField : public Field {
public:
    static const NumberField WORD_0;
    static const NumberField WORD_1;

    NumberField(uint8_t typeTag, DataReader& reader);
    NumberField(int64_t value, size_t size);
    explicit NumberField(int64_t value);

    int64_t getValue() const { return value_; }

    uint8_t getTypeTag() const override { return typeTag_; }
    uint8_t getArgumentTag() const override { return 0x06; }
    size_t getSize() const override { return size_; }
    std::span<const uint8_t> getBytes() const override { return buffer_; }
    std::string toString() const override;

private:
    uint8_t typeTag_;
    size_t size_;
    int64_t value_;
    std::vector<uint8_t> buffer_;
};

} // namespace beatlink::dbserver
