#pragma once

#include <asio.hpp>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace beatlink::dbserver {

class DataReader;

class Field {
public:
    virtual ~Field() = default;

    virtual std::span<const uint8_t> getBytes() const = 0;
    virtual size_t getSize() const = 0;
    virtual uint8_t getTypeTag() const = 0;
    virtual uint8_t getArgumentTag() const = 0;
    virtual std::string toString() const = 0;

    void write(asio::ip::tcp::socket& socket) const;

    static std::shared_ptr<Field> read(DataReader& reader);

protected:
    std::string getHexString() const;
};

using FieldPtr = std::shared_ptr<Field>;

} // namespace beatlink::dbserver
