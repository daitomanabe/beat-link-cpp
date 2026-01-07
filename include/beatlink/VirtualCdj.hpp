#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

#include <asio.hpp>

#include "Beat.hpp"
#include "CdjStatus.hpp"
#include "DeviceAnnouncement.hpp"
#include "DeviceFinder.hpp"
#include "DeviceReference.hpp"
#include "DeviceUpdate.hpp"
#include "DeviceUpdateListener.hpp"
#include "LifecycleListener.hpp"
#include "LifecycleParticipant.hpp"
#include "MasterListener.hpp"
#include "Metronome.hpp"
#include "MediaDetailsListener.hpp"
#include "MixerStatus.hpp"
#include "PacketTypes.hpp"
#include "PlayerSettings.hpp"
#include "Util.hpp"

namespace beatlink {

class BeatSender;

class VirtualCdj : public LifecycleParticipant {
public:
    static VirtualCdj& getInstance();

    bool isRunning() const override;
    bool isProxyingForVirtualRekordbox() const;
    bool inOpusQuadCompatibilityMode() const;

    bool start();
    bool start(uint8_t deviceNumber);
    void stop();

    asio::ip::address_v4 getLocalAddress() const;
    asio::ip::address_v4 getBroadcastAddress() const;

    void setUseStandardPlayerNumber(bool attempt);
    bool getUseStandardPlayerNumber() const;

    uint8_t getDeviceNumber() const;
    void setDeviceNumber(uint8_t number);

    const std::string& getDeviceName() const;
    void setDeviceName(const std::string& name);

    int getAnnounceInterval() const;
    void setAnnounceInterval(int interval);

    std::vector<std::shared_ptr<DeviceUpdate>> getLatestStatus() const;
    std::shared_ptr<DeviceUpdate> getLatestStatusFor(const DeviceUpdate& device) const;
    std::shared_ptr<DeviceUpdate> getLatestStatusFor(const DeviceAnnouncement& device) const;
    std::shared_ptr<DeviceUpdate> getLatestStatusFor(int deviceNumber) const;

    std::shared_ptr<DeviceUpdate> getTempoMaster() const;
    double getTempoEpsilon() const;
    void setTempoEpsilon(double epsilon);
    double getMasterTempo() const;

    void addMasterListener(const MasterListenerPtr& listener);
    void removeMasterListener(const MasterListenerPtr& listener);

    void addUpdateListener(const DeviceUpdateListenerPtr& listener);
    void removeUpdateListener(const DeviceUpdateListenerPtr& listener);

    void addMediaDetailsListener(const MediaDetailsListenerPtr& listener);
    void removeMediaDetailsListener(const MediaDetailsListenerPtr& listener);

    void processUpdate(const std::shared_ptr<DeviceUpdate>& update);
    void processBeat(const Beat& beat);

    void sendMediaQuery(int deviceNumber, TrackSourceSlot slot);

    void sendSyncModeCommand(int deviceNumber, bool synced);
    void sendSyncModeCommand(const DeviceUpdate& target, bool synced);

    void appointTempoMaster(int deviceNumber);
    void appointTempoMaster(const DeviceUpdate& target);

    void sendFaderStartCommand(const std::set<int>& deviceNumbersToStart,
                               const std::set<int>& deviceNumbersToStop);

    void sendOnAirCommand(const std::set<int>& deviceNumbersOnAir);
    void sendOnAirExtendedCommand(const std::set<int>& deviceNumbersOnAir);

    void sendLoadTrackCommand(int targetPlayer,
                              int rekordboxId,
                              int sourcePlayer,
                              TrackSourceSlot sourceSlot,
                              TrackType sourceType);
    void sendLoadTrackCommand(const DeviceUpdate& target,
                              int rekordboxId,
                              int sourcePlayer,
                              TrackSourceSlot sourceSlot,
                              TrackType sourceType);

    void sendLoadSettingsCommand(int targetPlayer, const PlayerSettings& settings);
    void sendLoadSettingsCommand(const DeviceUpdate& target, const PlayerSettings& settings);

    void setSendingStatus(bool send);
    bool isSendingStatus() const;

    void setPlaying(bool playing);
    bool isPlaying() const;
    Snapshot getPlaybackPosition() const;
    void adjustPlaybackPosition(int ms);

    void becomeTempoMaster();
    bool isTempoMaster() const;

    void setSynced(bool sync);
    bool isSynced() const;

    void setOnAir(bool audible);
    bool isOnAir() const;

    void setTempo(double bpm);
    double getTempo() const;

    void jumpToBeat(int beat);

    int64_t sendBeat();
    int64_t sendBeat(const Snapshot& snapshot);

    void handleSpecialAnnouncementPacket(PacketType kind,
                                         const uint8_t* data,
                                         size_t length,
                                         const asio::ip::address_v4& sender);

private:
    VirtualCdj();
    ~VirtualCdj();

    void receiverLoop();
    void announcerLoop();
    void sendAnnouncement();

    std::shared_ptr<DeviceUpdate> buildUpdate(const uint8_t* data,
                                              size_t length,
                                              const asio::ip::address_v4& sender);

    void deliverMasterChangedAnnouncement(const std::shared_ptr<DeviceUpdate>& update);
    void deliverTempoChangedAnnouncement(double tempo);
    void deliverBeatAnnouncement(const Beat& beat);
    void deliverDeviceUpdate(const std::shared_ptr<DeviceUpdate>& update);
    void deliverMediaDetails(const MediaDetails& details);
    void setMasterTempo(double tempo);
    void setTempoMaster(const std::shared_ptr<DeviceUpdate>& update);

    bool claimDeviceNumber();
    bool selfAssignDeviceNumber();
    void requestNumberFromMixer(const asio::ip::address_v4& mixerAddress);
    void defendDeviceNumber(const asio::ip::address_v4& invaderAddress);
    void handleDeviceClaimPacket(const uint8_t* data, size_t length, int deviceOffset,
                                 const asio::ip::address_v4& sender);

    void assembleAndSendPacket(PacketType type,
                               std::span<const uint8_t> payload,
                               const asio::ip::address_v4& target,
                               uint16_t port);
    void sendRawPacket(std::span<const uint8_t> packet,
                       const asio::ip::address_v4& target,
                       uint16_t port);

    void sendStatus();
    Snapshot avoidBeatPacket();
    void notifyBeatSenderOfChange();

    std::atomic<bool> running_{false};
    std::atomic<bool> starting_{false};
    std::atomic<bool> proxyingForVirtualRekordbox_{false};
    std::atomic<bool> inOpusQuadSQLiteMode_{false};

    std::atomic<bool> useStandardPlayerNumber_{false};
    std::atomic<int> announceInterval_{1500};

    std::array<uint8_t, 0x36> keepAliveBytes_{};
    std::string deviceName_ = "beat-link";

    std::atomic<int> claimingNumber_{0};
    std::atomic<bool> claimRejected_{false};
    std::atomic<int> mixerAssigned_{0};

    std::atomic<int> nextMaster_{0xff};
    std::atomic<int> masterYieldedFrom_{0};
    std::atomic<int> requestingMasterRoleFromPlayer_{0};

    std::atomic<int> largestSyncCounter_{0};
    std::atomic<int> syncCounter_{0};

    std::atomic<bool> master_{false};
    std::atomic<bool> synced_{false};
    std::atomic<bool> onAir_{false};

    std::atomic<int> packetCounter_{0};

    std::atomic<int> statusInterval_{200};

    std::atomic<bool> sendingStatus_{false};
    std::unique_ptr<std::thread> statusSenderThread_;

    Metronome metronome_{};
    std::atomic<bool> playing_{false};
    std::atomic<int64_t> whereStoppedBeat_{1};

    std::unique_ptr<BeatSender> beatSender_;
    MasterListenerPtr syncMasterListener_;

    std::atomic<double> tempo_{120.0};
    std::atomic<double> masterTempo_{0.0};
    std::atomic<double> tempoEpsilon_{0.0001};

    std::atomic<bool> haveMasterTempo_{false};

    mutable std::mutex updatesMutex_;
    std::unordered_map<DeviceReference, std::shared_ptr<DeviceUpdate>> updates_;
    std::shared_ptr<DeviceUpdate> tempoMaster_;

    mutable std::mutex listenersMutex_;
    std::vector<MasterListenerPtr> masterListeners_;
    std::vector<DeviceUpdateListenerPtr> updateListeners_;
    std::vector<MediaDetailsListenerPtr> mediaDetailsListeners_;

    asio::io_context ioContext_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    std::thread receiverThread_;
    std::thread announcerThread_;

    std::atomic<uint32_t> broadcastAddress_{0};
    std::atomic<uint32_t> localAddress_{0};

    LifecycleListenerPtr deviceFinderLifecycleListener_;
    LifecycleListenerPtr virtualRekordboxLifecycleListener_;
};

} // namespace beatlink
