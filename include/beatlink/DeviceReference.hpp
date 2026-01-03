#pragma once

#include <cstdint>
#include <functional>
#include <asio.hpp>

#include "DeviceAnnouncement.hpp"

namespace beatlink {

/**
 * A compact reference to a device on the network, used as a key in maps.
 * Ported from org.deepsymmetry.beatlink.DeviceReference
 */
class DeviceReference {
public:
    DeviceReference(const asio::ip::address_v4& address, int deviceNumber)
        : address_(address)
        , deviceNumber_(deviceNumber)
    {
    }

    /**
     * Create from a DeviceAnnouncement.
     */
    static DeviceReference fromAnnouncement(const DeviceAnnouncement& announcement) {
        return DeviceReference(announcement.getAddress(), announcement.getDeviceNumber());
    }

    asio::ip::address_v4 getAddress() const { return address_; }
    int getDeviceNumber() const { return deviceNumber_; }

    bool operator==(const DeviceReference& other) const {
        return address_ == other.address_ && deviceNumber_ == other.deviceNumber_;
    }

    bool operator!=(const DeviceReference& other) const {
        return !(*this == other);
    }

private:
    asio::ip::address_v4 address_;
    int deviceNumber_;
};

} // namespace beatlink

// Hash function for DeviceReference
namespace std {
template <>
struct hash<beatlink::DeviceReference> {
    size_t operator()(const beatlink::DeviceReference& ref) const {
        size_t h1 = std::hash<uint32_t>{}(ref.getAddress().to_uint());
        size_t h2 = std::hash<int>{}(ref.getDeviceNumber());
        return h1 ^ (h2 << 1);
    }
};
} // namespace std
