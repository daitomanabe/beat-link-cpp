#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <chrono>
#include <asio.hpp>

#include "Util.hpp"

namespace beatlink {

/**
 * Represents a device announcement seen on a DJ Link network.
 * Ported from org.deepsymmetry.beatlink.DeviceAnnouncement
 */
class DeviceAnnouncement {
public:
    /**
     * The expected size of a device announcement packet.
     */
    static constexpr size_t PACKET_SIZE = 0x36; // 54 bytes

    /**
     * Special device names for Opus Quad and XDJ-AZ.
     */
    static constexpr const char* OPUS_NAME = "OPUS-QUAD";
    static constexpr const char* XDJ_AZ_NAME = "XDJ-AZ";

    /**
     * Construct from raw packet data.
     *
     * @param data the packet bytes
     * @param length the length of the packet
     * @param senderAddress the IP address of the sender
     * @throws std::invalid_argument if packet size is wrong
     */
    DeviceAnnouncement(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress)
        : address_(senderAddress)
        , timestamp_(std::chrono::steady_clock::now())
    {
        if (length != PACKET_SIZE) {
            throw std::invalid_argument("Device announcement packet must be 54 bytes long");
        }

        packetBytes_.assign(data, data + length);
        name_ = Util::extractString(data, 0x0c, 20);
        isOpusQuad_ = (name_ == OPUS_NAME);
        isXdjAz_ = (name_ == XDJ_AZ_NAME);
        number_ = data[0x24];
    }

    /**
     * Construct with a custom device number.
     */
    DeviceAnnouncement(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress, int deviceNumber)
        : DeviceAnnouncement(data, length, senderAddress)
    {
        number_ = deviceNumber;
    }

    /**
     * Get the IP address of the device.
     */
    asio::ip::address_v4 getAddress() const { return address_; }

    /**
     * Get the timestamp when this announcement was received.
     */
    std::chrono::steady_clock::time_point getTimestamp() const { return timestamp_; }

    /**
     * Get the device name.
     */
    const std::string& getDeviceName() const { return name_; }

    /**
     * Get the device/player number.
     */
    int getDeviceNumber() const { return number_; }

    /**
     * Get the MAC address of the device.
     */
    std::array<uint8_t, 6> getHardwareAddress() const {
        std::array<uint8_t, 6> result;
        std::copy(packetBytes_.begin() + 0x26, packetBytes_.begin() + 0x2c, result.begin());
        return result;
    }

    /**
     * Get the MAC address as a formatted string.
     */
    std::string getHardwareAddressString() const {
        auto mac = getHardwareAddress();
        char buffer[18];
        snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return std::string(buffer);
    }

    /**
     * Get the number of peer devices this one currently sees.
     */
    int getPeerCount() const {
        return packetBytes_[0x30];
    }

    /**
     * Get the raw packet bytes.
     */
    const std::vector<uint8_t>& getPacketBytes() const { return packetBytes_; }

    /**
     * Check if this is an Opus Quad.
     */
    bool isOpusQuad() const { return isOpusQuad_; }

    /**
     * Check if this is an XDJ-AZ.
     */
    bool isXdjAz() const { return isXdjAz_; }

    /**
     * Check if this device uses Device Library Plus (SQLite) track IDs.
     */
    bool isUsingDeviceLibraryPlus() const { return isOpusQuad_ || isXdjAz_; }

    /**
     * Get a string representation.
     */
    std::string toString() const {
        return "DeviceAnnouncement[device:" + std::to_string(number_) +
               ", name:" + name_ +
               ", address:" + address_.to_string() +
               ", peers:" + std::to_string(getPeerCount()) + "]";
    }

    /**
     * Get the age of this announcement in milliseconds.
     */
    int64_t getAge() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp_).count();
    }

private:
    asio::ip::address_v4 address_;
    std::chrono::steady_clock::time_point timestamp_;
    std::string name_;
    int number_;
    std::vector<uint8_t> packetBytes_;
    bool isOpusQuad_;
    bool isXdjAz_;
};

} // namespace beatlink
