#pragma once

#include "Field.hpp"

#include <vector>

namespace beatlink::dbserver {

class DataReader;

class BinaryField : public Field {
public:
    explicit BinaryField(DataReader& reader);
    explicit BinaryField(const std::vector<uint8_t>& bytes);
    explicit BinaryField(std::span<const uint8_t> bytes);

    std::span<const uint8_t> getValue() const { return value_; }
    std::vector<uint8_t> getValueAsArray() const { return value_; }

    uint8_t getTypeTag() const override { return typeTag_; }
    uint8_t getArgumentTag() const override { return 0x03; }
    size_t getSize() const override { return size_; }
    std::span<const uint8_t> getBytes() const override { return buffer_; }
    std::string toString() const override;

private:
    uint8_t typeTag_ = 0x14;
    size_t size_ = 0;
    std::vector<uint8_t> value_;
    std::vector<uint8_t> buffer_;
};

} // namespace beatlink::dbserver
