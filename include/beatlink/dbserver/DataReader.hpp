#pragma once

#include <asio.hpp>
#include <chrono>
#include <cstdint>

namespace beatlink::dbserver {

class DataReader {
public:
    DataReader(asio::ip::tcp::socket& socket, std::chrono::milliseconds timeout);

    uint8_t readByte();
    void readFully(uint8_t* data, size_t length);

private:
    asio::ip::tcp::socket& socket_;
    std::chrono::milliseconds timeout_;
};

} // namespace beatlink::dbserver
