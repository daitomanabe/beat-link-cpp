#include "beatlink/dbserver/NumberField.hpp"
#include "beatlink/dbserver/DataReader.hpp"

#include <stdexcept>

namespace beatlink::dbserver {

const NumberField NumberField::WORD_0 = NumberField(0, 4);
const NumberField NumberField::WORD_1 = NumberField(1, 4);

NumberField::NumberField(uint8_t typeTag, DataReader& reader)
    : typeTag_(typeTag)
    , size_(0)
    , value_(0)
{
    switch (typeTag_) {
        case 0x0f:
            size_ = 1;
            break;
        case 0x10:
            size_ = 2;
            break;
        case 0x11:
            size_ = 4;
            break;
        default:
            throw std::invalid_argument("NumberField cannot have tag " + std::to_string(typeTag_));
    }

    buffer_.resize(size_ + 1);
    buffer_[0] = typeTag_;
    reader.readFully(buffer_.data() + 1, size_);
    value_ = beatlink::Util::bytesToNumber(buffer_.data(), 1, size_);
}

NumberField::NumberField(int64_t value, size_t size)
    : typeTag_(0)
    , size_(size)
    , value_(value & 0xffffffffLL)
{
    buffer_.resize(size_ + 1);
    switch (size_) {
        case 1:
            typeTag_ = 0x0f;
            buffer_[1] = static_cast<uint8_t>(value_ & 0xff);
            break;
        case 2:
            typeTag_ = 0x10;
            buffer_[1] = static_cast<uint8_t>((value_ >> 8) & 0xff);
            buffer_[2] = static_cast<uint8_t>(value_ & 0xff);
            break;
        case 4:
            typeTag_ = 0x11;
            buffer_[1] = static_cast<uint8_t>((value_ >> 24) & 0xff);
            buffer_[2] = static_cast<uint8_t>((value_ >> 16) & 0xff);
            buffer_[3] = static_cast<uint8_t>((value_ >> 8) & 0xff);
            buffer_[4] = static_cast<uint8_t>(value_ & 0xff);
            break;
        default:
            throw std::invalid_argument("NumberField cannot have size " + std::to_string(size_));
    }
    buffer_[0] = typeTag_;
}

NumberField::NumberField(int64_t value)
    : NumberField(value, 4) {
}

std::string NumberField::toString() const {
    return "NumberField[ size: " + std::to_string(size_) + ", value: " + std::to_string(value_) +
           ", bytes: " + getHexString() + "]";
}

} // namespace beatlink::dbserver
