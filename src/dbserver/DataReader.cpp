#include "beatlink/dbserver/DataReader.hpp"

#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace beatlink::dbserver {

DataReader::DataReader(asio::ip::tcp::socket& socket, std::chrono::milliseconds timeout)
    : socket_(socket)
    , timeout_(timeout)
{
}

uint8_t DataReader::readByte() {
    uint8_t value = 0;
    readFully(&value, 1);
    return value;
}

void DataReader::readFully(uint8_t* data, size_t length) {
    if (length == 0) {
        return;
    }

    size_t totalRead = 0;
    auto deadline = std::chrono::steady_clock::now() + timeout_;
    auto socketHandle = socket_.native_handle();

    while (totalRead < length) {
        if (timeout_.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                throw std::runtime_error("Timed out reading from dbserver socket");
            }

            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            timeval tv{};
            tv.tv_sec = static_cast<long>(remaining.count() / 1000);
            tv.tv_usec = static_cast<long>((remaining.count() % 1000) * 1000);

            fd_set readfds;
            FD_ZERO(&readfds);
#ifdef _WIN32
            FD_SET(socketHandle, &readfds);
            int ready = ::select(0, &readfds, nullptr, nullptr, &tv);
#else
            FD_SET(socketHandle, &readfds);
            int ready = ::select(socketHandle + 1, &readfds, nullptr, nullptr, &tv);
#endif
            if (ready == 0) {
                throw std::runtime_error("Timed out reading from dbserver socket");
            }
            if (ready < 0) {
                throw std::runtime_error("Error waiting for dbserver socket readiness");
            }
        }

#ifdef _WIN32
        int received = ::recv(socketHandle, reinterpret_cast<char*>(data + totalRead),
                              static_cast<int>(length - totalRead), 0);
#else
        ssize_t received = ::recv(socketHandle, data + totalRead, length - totalRead, 0);
#endif
        if (received <= 0) {
            throw std::runtime_error("Failed to read from dbserver socket");
        }
        totalRead += static_cast<size_t>(received);
    }
}

} // namespace beatlink::dbserver
