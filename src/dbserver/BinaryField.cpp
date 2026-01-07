#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/DataReader.hpp"
#include "beatlink/Util.hpp"

namespace beatlink::dbserver {

BinaryField::BinaryField(DataReader& reader) {
    uint8_t sizeBytes[4] = {};
    reader.readFully(sizeBytes, sizeof(sizeBytes));
    size_ = static_cast<size_t>(beatlink::Util::bytesToNumber(sizeBytes, 0, 4));

    if (size_ > 0) {
        buffer_.resize(size_ + 5);
        buffer_[0] = typeTag_;
        std::copy(sizeBytes, sizeBytes + 4, buffer_.begin() + 1);
        reader.readFully(buffer_.data() + 5, size_);
        value_.assign(buffer_.begin() + 5, buffer_.end());
    } else {
        buffer_.clear();
        value_.clear();
    }
}

BinaryField::BinaryField(const std::vector<uint8_t>& bytes)
    : size_(bytes.size())
    , value_(bytes)
{
    if (size_ > 0) {
        buffer_.resize(size_ + 5);
        buffer_[0] = typeTag_;
        beatlink::Util::numberToBytes(static_cast<int>(size_), buffer_.data(), buffer_.size(), 1, 4);
        std::copy(bytes.begin(), bytes.end(), buffer_.begin() + 5);
    } else {
        buffer_.clear();
    }
}

BinaryField::BinaryField(std::span<const uint8_t> bytes)
    : BinaryField(std::vector<uint8_t>(bytes.begin(), bytes.end()))
{
}

std::string BinaryField::toString() const {
    return "BinaryField[ size: " + std::to_string(size_) + ", bytes: " + getHexString() + "]";
}

} // namespace beatlink::dbserver
