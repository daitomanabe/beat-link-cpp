#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>
#include <asio.hpp>

#include "DeviceAnnouncement.hpp"
#include "DeviceReference.hpp"
#include "PacketTypes.hpp"
#include "Util.hpp"

namespace beatlink {

/**
 * Callback type for device announcement events.
 */
using DeviceAnnouncementCallback = std::function<void(const DeviceAnnouncement&)>;

/**
 * Watches for devices to report their presence by broadcasting announcement packets on port 50000.
 * Ported from org.deepsymmetry.beatlink.DeviceFinder
 */
class DeviceFinder {
public:
    /**
     * The maximum age in milliseconds before a device is considered gone.
     */
    static constexpr int64_t MAXIMUM_AGE = 10000;

    /**
     * Get the singleton instance.
     */
    static DeviceFinder& getInstance() {
        static DeviceFinder instance;
        return instance;
    }

    /**
     * Check if the finder is running.
     */
    bool isRunning() const { return running_.load(); }

    /**
     * Start listening for device announcements.
     *
     * @return true if started successfully
     */
    bool start();

    /**
     * Stop listening for device announcements.
     */
    void stop();

    /**
     * Get the timestamp when we started listening.
     */
    std::chrono::steady_clock::time_point getStartTime() const {
        return startTime_;
    }

    /**
     * Get the timestamp when we saw the first device.
     */
    std::chrono::steady_clock::time_point getFirstDeviceTime() const {
        return firstDeviceTime_;
    }

    /**
     * Get the current set of devices.
     */
    std::vector<DeviceAnnouncement> getCurrentDevices();

    /**
     * Find a device by its device number.
     */
    std::optional<DeviceAnnouncement> getLatestAnnouncementFrom(int deviceNumber);

    /**
     * Add an address to ignore (e.g., our own VirtualCdj address).
     */
    void addIgnoredAddress(const asio::ip::address_v4& address);

    /**
     * Remove an address from the ignore list.
     */
    void removeIgnoredAddress(const asio::ip::address_v4& address);

    /**
     * Check if an address is being ignored.
     */
    bool isAddressIgnored(const asio::ip::address_v4& address) const;

    /**
     * Add a listener for device found events.
     */
    void addDeviceFoundListener(DeviceAnnouncementCallback callback);

    /**
     * Add a listener for device lost events.
     */
    void addDeviceLostListener(DeviceAnnouncementCallback callback);

    /**
     * Clear all listeners.
     */
    void clearListeners();

    // Delete copy/move
    DeviceFinder(const DeviceFinder&) = delete;
    DeviceFinder& operator=(const DeviceFinder&) = delete;

private:
    DeviceFinder();
    ~DeviceFinder();

    void receiverLoop();
    void processPacket(const uint8_t* data, size_t length, const asio::ip::address_v4& sender);
    void processAnnouncement(const DeviceAnnouncement& announcement);
    void createAndProcessOpusAnnouncements(const uint8_t* data, size_t length, const asio::ip::address_v4& sender);
    void expireDevices();
    bool isDeviceNew(const DeviceAnnouncement& announcement);
    void updateDevices(const DeviceAnnouncement& announcement);
    void deliverFoundAnnouncement(const DeviceAnnouncement& announcement);
    void deliverLostAnnouncement(const DeviceAnnouncement& announcement);

    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point firstDeviceTime_;

    asio::io_context ioContext_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    std::thread receiverThread_;

    mutable std::mutex devicesMutex_;
    std::unordered_map<DeviceReference, DeviceAnnouncement> devices_;

    mutable std::mutex ignoredMutex_;
    std::unordered_set<uint32_t> ignoredAddresses_;

    mutable std::mutex listenersMutex_;
    std::vector<DeviceAnnouncementCallback> foundListeners_;
    std::vector<DeviceAnnouncementCallback> lostListeners_;

    std::array<uint8_t, 512> receiveBuffer_;
    asio::ip::udp::endpoint senderEndpoint_;
};

} // namespace beatlink
