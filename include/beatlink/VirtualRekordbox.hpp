#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <asio.hpp>

#include "CdjStatus.hpp"
#include "DeviceFinder.hpp"
#include "DeviceReference.hpp"
#include "DeviceUpdate.hpp"
#include "DeviceUpdateListener.hpp"
#include "LifecycleParticipant.hpp"
#include "PacketTypes.hpp"
#include "Util.hpp"
#include "data/OpusProvider.hpp"
#include "data/SlotReference.hpp"

namespace beatlink {

class VirtualRekordbox : public LifecycleParticipant {
public:
    static VirtualRekordbox& getInstance();

    bool isRunning() const override;

    asio::ip::address_v4 getLocalAddress() const;
    asio::ip::address_v4 getBroadcastAddress() const;

    uint8_t getDeviceNumber() const;
    void setDeviceNumber(uint8_t number);

    int getAnnounceInterval() const;
    void setAnnounceInterval(int interval);

    static std::string getDeviceName();

    void requestPssi();
    void clearPlayerCaches(int usbSlotNumber);

    std::optional<data::SlotReference> findMatchedTrackSourceSlotForPlayer(int player) const;
    int findDeviceSqlRekordboxIdForPlayer(int player) const;

    std::optional<asio::ip::address_v4> getMatchedAddress() const;
    std::vector<std::string> getMatchingInterfaces() const;

    void sendRekordboxLightingPacket();
    void sendRekordboxAnnouncement();

    bool start();
    void stop();

    std::shared_ptr<DeviceUpdate> getLatestStatusFor(int deviceNumber) const;

    void addUpdateListener(const DeviceUpdateListenerPtr& listener);
    void removeUpdateListener(const DeviceUpdateListenerPtr& listener);
    std::vector<DeviceUpdateListenerPtr> getUpdateListeners() const;

    std::string toString() const;

private:
    VirtualRekordbox();
    ~VirtualRekordbox();

    struct PacketTracker {
        std::vector<uint8_t> data;

        bool receivePacket(const std::vector<uint8_t>& packetData, int packetNumber, int totalPackets);
        void resetForNewPacketStream();
        std::vector<uint8_t> getDataAsBytesAndTrimTrailingZeros() const;
    };

    bool createVirtualRekordbox();
    bool selfAssignDeviceNumber();
    void receiverLoop();
    void announcerLoop();
    void sendAnnouncements();

    std::shared_ptr<DeviceUpdate> buildUpdate(const uint8_t* data,
                                              size_t length,
                                              const asio::ip::address_v4& sender);
    void processUpdate(const std::shared_ptr<DeviceUpdate>& update);
    void deliverDeviceUpdate(const std::shared_ptr<DeviceUpdate>& update);

    std::atomic<bool> running_{false};
    std::atomic<int> announceInterval_{1500};

    std::atomic<uint32_t> broadcastAddress_{0};
    std::atomic<uint32_t> localAddress_{0};
    std::atomic<int> matchedPrefixLength_{0};

    std::vector<std::string> matchingInterfaces_;
    std::optional<asio::ip::address_v4> matchedAddress_;

    std::unordered_map<DeviceReference, std::shared_ptr<DeviceUpdate>> updates_;
    mutable std::mutex updatesMutex_;

    std::unordered_map<int, data::SlotReference> playerTrackSourceSlots_;
    std::unordered_map<int, int> playerToDeviceSqlRekordboxId_;
    std::unordered_map<int, uint8_t> lastValidStatusFlagBytes_;
    std::unordered_map<int, int> previousRawRekordboxIds_;
    std::unordered_map<int, PacketTracker> playerPacketTrackers_;
    mutable std::mutex opusMutex_;

    mutable std::mutex listenersMutex_;
    std::vector<DeviceUpdateListenerPtr> updateListeners_;

    std::array<uint8_t, 0x36> rekordboxKeepAliveBytes_{};
    std::array<uint8_t, 0x128> rekordboxLightingRequestStatusBytes_{};

    asio::io_context ioContext_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    std::thread receiverThread_;
    std::thread announcerThread_;

    LifecycleListenerPtr deviceFinderLifecycleListener_;
};

} // namespace beatlink
