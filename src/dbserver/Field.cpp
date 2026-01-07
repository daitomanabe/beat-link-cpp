#include "beatlink/dbserver/Field.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/DataReader.hpp"
#include "beatlink/dbserver/NumberField.hpp"
#include "beatlink/dbserver/StringField.hpp"

#include <asio.hpp>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace beatlink::dbserver {

void Field::write(asio::ip::tcp::socket& socket) const {
    auto bytes = getBytes();
    if (bytes.empty()) {
        return;
    }
    asio::write(socket, asio::buffer(bytes.data(), bytes.size()));
}

std::shared_ptr<Field> Field::read(DataReader& reader) {
    const uint8_t tag = reader.readByte();
    switch (tag) {
        case 0x0f:
        case 0x10:
        case 0x11:
            return std::make_shared<NumberField>(tag, reader);
        case 0x14:
            return std::make_shared<BinaryField>(reader);
        case 0x26:
            return std::make_shared<StringField>(reader);
        default:
            throw std::runtime_error("Unable to read a field with type tag " + std::to_string(tag));
    }
}

std::string Field::getHexString() const {
    auto bytes = getBytes();
    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
        oss << ' ';
    }
    return oss.str();
}

} // namespace beatlink::dbserver
