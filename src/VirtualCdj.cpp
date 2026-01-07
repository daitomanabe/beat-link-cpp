#include "beatlink/VirtualCdj.hpp"

#include "beatlink/BeatFinder.hpp"
#include "beatlink/MediaDetails.hpp"
#include "beatlink/VirtualRekordbox.hpp"
#include "beatlink/data/OpusProvider.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <format>
#include <iostream>
#include <optional>

#if defined(__APPLE__) || defined(__linux__)
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#if defined(__APPLE__)
#include <net/if_dl.h>
#else
#include <netpacket/packet.h>
#endif
#endif

namespace beatlink {
namespace {

constexpr size_t kDeviceNameOffset = 0x0c;
constexpr size_t kDeviceNameLength = 0x14;
constexpr size_t kDeviceNumberOffset = 0x24;
constexpr size_t kMacAddressOffset = 0x26;
constexpr size_t kIpAddressOffset = 0x2c;

constexpr int kSelfAssignmentWatchPeriodMs = 4000;

constexpr std::array<uint8_t, 0x26> kHelloBytes = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x0a, 0x00, 0x62, 0x65, 0x61, 0x74,
    0x2d, 0x6c, 0x69, 0x6e, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x00, 0x26, 0x01, 0x40
};

constexpr std::array<uint8_t, 0x2c> kClaimStage1 = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x00, 0x00, 0x62, 0x65, 0x61, 0x74,
    0x2d, 0x6c, 0x69, 0x6e, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x00, 0x2c, 0x0d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x32> kClaimStage2 = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x02, 0x00, 0x62, 0x65, 0x61, 0x74,
    0x2d, 0x6c, 0x69, 0x6e, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00,
    0x01, 0x00
};

constexpr std::array<uint8_t, 0x26> kClaimStage3 = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x04, 0x00, 0x62, 0x65, 0x61, 0x74,
    0x2d, 0x6c, 0x69, 0x6e, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x00, 0x26, 0x0d, 0x00
};

constexpr std::array<uint8_t, 0x32> kAssignmentRequestBytes = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x02, 0x01, 0x62, 0x65, 0x61, 0x74,
    0x2d, 0x6c, 0x69, 0x6e, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00
};

constexpr std::array<uint8_t, 0x29> kDeviceNumberDefenseBytes = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x08, 0x00, 0x62, 0x65, 0x61, 0x74,
    0x2d, 0x6c, 0x69, 0x6e, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x11> kMediaQueryPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x0d> kSyncControlPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x0f
};

constexpr std::array<uint8_t, 0x09> kFaderStartPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x04, 0x02, 0x02, 0x02, 0x02
};

constexpr std::array<uint8_t, 0x0e> kChannelsOnAirPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x16> kChannelsOnAirExtendedPayload = {
    0x01, 0x03, 0x0d, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x39> kLoadTrackPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x34, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x0d> kYieldAckPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x01
};

constexpr std::array<uint8_t, 0x55> kLoadSettingsPayload = {
    0x02, 0x0d, 0x0d, 0x00, 0x50, 0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x41> kBeatPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x3c, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x10, 0x10, 0x10,
    0x10, 0x04, 0x04, 0x04, 0x04, 0x20, 0x20, 0x20, 0x20, 0x08, 0x08, 0x08, 0x08, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00,
    0x0d
};

constexpr std::array<uint8_t, 0x09> kMasterHandoffRequestPayload = {
    0x01, 0x00, 0x0d, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0d
};

constexpr std::array<uint8_t, 0xfd> kStatusPayload = {
    0x01,
    0x04, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x01, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x2e, 0x34, 0x33,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x10, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x01, 0x00, 0x00,
    0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x07, 0x61, 0x00, 0x00, 0x06, 0x2f
};

} // namespace

class BeatSender {
public:
    explicit BeatSender(VirtualCdj& owner, Metronome& metronome)
        : owner_(owner)
        , metronome_(metronome)
    {
        thread_ = std::thread([this]() { run(); });
    }

    ~BeatSender() {
        shutDown();
    }

    void timelineChanged() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            changed_ = true;
        }
        cv_.notify_one();
    }

    void shutDown() {
        running_.store(false);
        cv_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    static constexpr int64_t kBeatThresholdMs = 10;
    static constexpr int64_t kSleepThresholdMs = 5;

private:
    void run() {
        while (running_.load()) {
            Snapshot snapshot = metronome_.getSnapshot();

            if (lastBeatSent_.has_value()) {
                if (snapshot.getBeatPhase() > 0.5 || snapshot.getBeat() != *lastBeatSent_) {
                    lastBeatSent_.reset();
                }
            }

            const int64_t currentBeatDue = snapshot.getTimeOfBeat(snapshot.getBeat());
            const int64_t nextBeatDue = snapshot.getTimeOfBeat(snapshot.getBeat() + 1);
            const int64_t distanceIntoBeat = snapshot.getInstant() - currentBeatDue;

            if (distanceIntoBeat < kBeatThresholdMs &&
                (!lastBeatSent_.has_value() || *lastBeatSent_ != snapshot.getBeat())) {
                lastBeatSent_ = owner_.sendBeat(snapshot);
            }

            int64_t sleepMs = nextBeatDue - snapshot.getInstant();
            if (sleepMs < 0) {
                sleepMs = 0;
            }

            std::unique_lock<std::mutex> lock(mutex_);
            if (!running_.load()) {
                break;
            }
            if (sleepMs > kSleepThresholdMs) {
                cv_.wait_for(lock, std::chrono::milliseconds(sleepMs - kSleepThresholdMs), [this]() { return changed_ || !running_.load(); });
            } else {
                cv_.wait_for(lock, std::chrono::milliseconds(sleepMs), [this]() { return changed_ || !running_.load(); });
            }
            changed_ = false;
        }
    }

    VirtualCdj& owner_;
    Metronome& metronome_;
    std::atomic<bool> running_{true};
    std::optional<int64_t> lastBeatSent_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool changed_{false};
};

namespace {

std::optional<asio::ip::address_v4> findBroadcastAddress(asio::ip::address_v4 localAddress) {
#if defined(__APPLE__) || defined(__linux__)
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0) {
        return std::nullopt;
    }
    std::optional<std::string> interfaceName;
    for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        auto* addr = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
        uint32_t addrHost = ntohl(addr->sin_addr.s_addr);
        if (addrHost == localAddress.to_uint()) {
            interfaceName = ifa->ifa_name;
            break;
        }
    }
    if (interfaceName) {
        for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            if (interfaceName.value() != ifa->ifa_name) {
                continue;
            }
            auto* addr = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            auto* mask = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
            uint32_t addrHost = ntohl(addr->sin_addr.s_addr);
            uint32_t maskHost = ntohl(mask->sin_addr.s_addr);
            uint32_t broadcastHost = addrHost | ~maskHost;
            freeifaddrs(ifaddr);
            return asio::ip::address_v4(broadcastHost);
        }
    }
    freeifaddrs(ifaddr);
#endif
    return std::nullopt;
}

bool fillMacAddress(asio::ip::address_v4 localAddress, std::array<uint8_t, 6>& macOut) {
#if defined(__APPLE__) || defined(__linux__)
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0) {
        return false;
    }
    std::optional<std::string> interfaceName;
    for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        auto* addr = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
        uint32_t addrHost = ntohl(addr->sin_addr.s_addr);
        if (addrHost == localAddress.to_uint()) {
            interfaceName = ifa->ifa_name;
            break;
        }
    }
    if (!interfaceName) {
        freeifaddrs(ifaddr);
        return false;
    }
    for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
#if defined(__APPLE__)
        if (ifa->ifa_addr->sa_family != AF_LINK) {
            continue;
        }
        if (interfaceName.value() != ifa->ifa_name) {
            continue;
        }
        auto* sdl = reinterpret_cast<sockaddr_dl*>(ifa->ifa_addr);
        if (sdl->sdl_alen < 6) {
            continue;
        }
        const auto* mac = reinterpret_cast<uint8_t*>(LLADDR(sdl));
        std::copy(mac, mac + 6, macOut.begin());
        freeifaddrs(ifaddr);
        return true;
#else
        if (ifa->ifa_addr->sa_family != AF_PACKET) {
            continue;
        }
        if (interfaceName.value() != ifa->ifa_name) {
            continue;
        }
        auto* sll = reinterpret_cast<sockaddr_ll*>(ifa->ifa_addr);
        if (sll->sll_halen < 6) {
            continue;
        }
        std::copy(sll->sll_addr, sll->sll_addr + 6, macOut.begin());
        freeifaddrs(ifaddr);
        return true;
#endif
    }
    freeifaddrs(ifaddr);
#endif
    return false;
}

} // namespace

VirtualCdj& VirtualCdj::getInstance() {
    static VirtualCdj instance;
    return instance;
}

VirtualCdj::VirtualCdj() {
    std::copy(kHelloBytes.begin(), kHelloBytes.end(), keepAliveBytes_.begin());
    setDeviceName(deviceName_);

    deviceFinderLifecycleListener_ = std::make_shared<LifecycleCallbacks>(
        [](LifecycleParticipant&) {},
        [](LifecycleParticipant&) {
            if (VirtualCdj::getInstance().isRunning()) {
                VirtualCdj::getInstance().stop();
            }
        });

    virtualRekordboxLifecycleListener_ = std::make_shared<LifecycleCallbacks>(
        [](LifecycleParticipant&) {},
        [](LifecycleParticipant&) {
            if (VirtualCdj::getInstance().isProxyingForVirtualRekordbox()) {
                VirtualCdj::getInstance().stop();
            }
        });

    class SyncListenerImpl final : public SyncListener {
    public:
        explicit SyncListenerImpl(VirtualCdj& owner) : owner_(owner) {}
        void setSyncMode(bool synced) override { owner_.setSynced(synced); }
        void becomeMaster() override {
            if (owner_.isSendingStatus()) {
                std::thread([this]() {
                    try {
                        owner_.becomeTempoMaster();
                    } catch (...) {
                    }
                }).detach();
            }
        }
    private:
        VirtualCdj& owner_;
    };

    class OnAirListenerImpl final : public OnAirListener {
    public:
        explicit OnAirListenerImpl(VirtualCdj& owner) : owner_(owner) {}
        void channelsOnAir(const std::set<int>& audibleChannels) override {
            owner_.setOnAir(audibleChannels.count(owner_.getDeviceNumber()) > 0);
        }
    private:
        VirtualCdj& owner_;
    };

    class FaderStartListenerImpl final : public FaderStartListener {
    public:
        explicit FaderStartListenerImpl(VirtualCdj& owner) : owner_(owner) {}
        void fadersChanged(const std::set<int>& playersToStart,
                           const std::set<int>& playersToStop) override {
            const int num = owner_.getDeviceNumber();
            if (playersToStart.count(num) > 0) {
                owner_.setPlaying(true);
            } else if (playersToStop.count(num) > 0) {
                owner_.setPlaying(false);
            }
        }
    private:
        VirtualCdj& owner_;
    };

    class MasterHandoffListenerImpl final : public MasterHandoffListener {
    public:
        explicit MasterHandoffListenerImpl(VirtualCdj& owner) : owner_(owner) {}
        void yieldMasterTo(int deviceNumber) override {
            if (!owner_.isTempoMaster()) {
                return;
            }
            if (!owner_.isSendingStatus()) {
                return;
            }
            if (owner_.getDeviceNumber() == deviceNumber) {
                return;
            }
            owner_.nextMaster_.store(deviceNumber);
            auto targetStatus = owner_.getLatestStatusFor(deviceNumber);
            if (!targetStatus) {
                return;
            }
            std::array<uint8_t, kYieldAckPayload.size()> payload = kYieldAckPayload;
            payload[2] = owner_.getDeviceNumber();
            payload[8] = owner_.getDeviceNumber();
            owner_.assembleAndSendPacket(PacketType::MASTER_HANDOFF_RESPONSE,
                                         payload,
                                         targetStatus->getAddress(),
                                         Ports::UPDATE);
        }

        void yieldResponse(int deviceNumber, bool yielded) override {
            if (!yielded) {
                return;
            }
            if (!owner_.isSendingStatus()) {
                return;
            }
            if (deviceNumber == owner_.requestingMasterRoleFromPlayer_.load()) {
                owner_.requestingMasterRoleFromPlayer_.store(0);
                owner_.masterYieldedFrom_.store(deviceNumber);
            }
        }
    private:
        VirtualCdj& owner_;
    };

    class SyncMasterListenerImpl final : public MasterListener {
    public:
        explicit SyncMasterListenerImpl(VirtualCdj& owner) : owner_(owner) {}
        void masterChanged(const DeviceUpdate* /*update*/) override {}
        void tempoChanged(double tempo) override {
            if (!owner_.isTempoMaster()) {
                owner_.metronome_.setTempo(tempo);
                owner_.notifyBeatSenderOfChange();
            }
        }
        void newBeat(const Beat& /*beat*/) override {
            if (!owner_.isTempoMaster()) {
                owner_.metronome_.setBeatPhase(0.0);
                owner_.notifyBeatSenderOfChange();
            }
        }
    private:
        VirtualCdj& owner_;
    };

    syncMasterListener_ = std::make_shared<SyncMasterListenerImpl>(*this);

    BeatFinder::getInstance().addOnAirListener(std::make_shared<OnAirListenerImpl>(*this));
    BeatFinder::getInstance().addFaderStartListener(std::make_shared<FaderStartListenerImpl>(*this));
    BeatFinder::getInstance().addSyncListener(std::make_shared<SyncListenerImpl>(*this));
    BeatFinder::getInstance().addMasterHandoffListener(std::make_shared<MasterHandoffListenerImpl>(*this));
}

VirtualCdj::~VirtualCdj() {
    stop();
}

bool VirtualCdj::isRunning() const {
    return proxyingForVirtualRekordbox_.load() || (running_.load() && !starting_.load());
}

bool VirtualCdj::isProxyingForVirtualRekordbox() const {
    return proxyingForVirtualRekordbox_.load();
}

bool VirtualCdj::inOpusQuadCompatibilityMode() const {
    return proxyingForVirtualRekordbox_.load() || inOpusQuadSQLiteMode_.load();
}

bool VirtualCdj::start() {
    if (isRunning()) {
        return true;
    }

    starting_.store(true);

    DeviceFinder::getInstance().addLifecycleListener(deviceFinderLifecycleListener_);
    DeviceFinder::getInstance().start();
    bool haveDevices = false;
    for (int i = 0; i < 200; ++i) {
        if (!DeviceFinder::getInstance().getCurrentDevices().empty()) {
            haveDevices = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!haveDevices) {
        starting_.store(false);
        return false;
    }

    const auto devices = DeviceFinder::getInstance().getCurrentDevices();
    if (devices.empty()) {
        starting_.store(false);
        return false;
    }

    bool opusDetected = false;
    bool useVirtualRekordbox = false;
    bool useSqliteMode = false;

    for (const auto& device : devices) {
        if (!device.isOpusQuad()) {
            continue;
        }
        opusDetected = true;
        if (data::OpusProvider::getInstance().usingDeviceLibraryPlus()) {
            useSqliteMode = true;
        } else {
            useVirtualRekordbox = true;
        }
        break;
    }

    if (opusDetected && useVirtualRekordbox) {
        proxyingForVirtualRekordbox_.store(true);
        inOpusQuadSQLiteMode_.store(false);
        VirtualRekordbox::getInstance().addLifecycleListener(virtualRekordboxLifecycleListener_);
        bool success = VirtualRekordbox::getInstance().start();
        starting_.store(false);
        return success;
    }

    proxyingForVirtualRekordbox_.store(false);
    inOpusQuadSQLiteMode_.store(useSqliteMode);
    if (opusDetected && useSqliteMode) {
        data::OpusProvider::getInstance().start();
    }

    asio::ip::address_v4 deviceAddress = devices.front().getAddress();

    asio::ip::udp::socket probe(ioContext_);
    probe.open(asio::ip::udp::v4());
    probe.connect(asio::ip::udp::endpoint(deviceAddress, Ports::UPDATE));
    asio::ip::address_v4 localAddress = probe.local_endpoint().address().to_v4();
    probe.close();

    localAddress_.store(localAddress.to_uint());
    auto broadcast = findBroadcastAddress(localAddress).value_or(asio::ip::address_v4::broadcast());
    broadcastAddress_.store(broadcast.to_uint());

    std::array<uint8_t, 6> mac{};
    if (fillMacAddress(localAddress, mac)) {
        std::copy(mac.begin(), mac.end(), keepAliveBytes_.begin() + kMacAddressOffset);
    }
    auto localBytes = localAddress.to_bytes();
    keepAliveBytes_[kIpAddressOffset] = localBytes[0];
    keepAliveBytes_[kIpAddressOffset + 1] = localBytes[1];
    keepAliveBytes_[kIpAddressOffset + 2] = localBytes[2];
    keepAliveBytes_[kIpAddressOffset + 3] = localBytes[3];

    socket_ = std::make_unique<asio::ip::udp::socket>(ioContext_);
    socket_->open(asio::ip::udp::v4());
    socket_->set_option(asio::socket_base::reuse_address(true));
    socket_->set_option(asio::socket_base::broadcast(true));
    socket_->bind(asio::ip::udp::endpoint(localAddress, Ports::UPDATE));

    DeviceFinder::getInstance().addIgnoredAddress(localAddress);

    if (!claimDeviceNumber()) {
        DeviceFinder::getInstance().removeIgnoredAddress(localAddress);
        socket_->close();
        socket_.reset();
        starting_.store(false);
        return false;
    }

    running_.store(true);

    receiverThread_ = std::thread(&VirtualCdj::receiverLoop, this);
    announcerThread_ = std::thread(&VirtualCdj::announcerLoop, this);

    starting_.store(false);
    deliverLifecycleAnnouncement(true);
    return true;
}

bool VirtualCdj::start(uint8_t deviceNumber) {
    if (!isRunning()) {
        setDeviceNumber(deviceNumber);
        return start();
    }
    return true;
}

void VirtualCdj::stop() {
    if (!isRunning()) {
        return;
    }

    const bool wasProxying = proxyingForVirtualRekordbox_.load();
    const bool wasOpusMode = inOpusQuadCompatibilityMode();

    if (wasProxying) {
        try {
            VirtualRekordbox::getInstance().stop();
        } catch (...) {
        }
    }
    if (wasOpusMode) {
        if (data::OpusProvider::getInstance().isRunning()) {
            data::OpusProvider::getInstance().stop();
        }
    }

    proxyingForVirtualRekordbox_.store(false);
    inOpusQuadSQLiteMode_.store(false);

    running_.store(false);

    if (socket_) {
        asio::error_code ec;
        socket_->close(ec);
    }

    if (receiverThread_.joinable()) {
        receiverThread_.join();
    }
    if (announcerThread_.joinable()) {
        announcerThread_.join();
    }

    if (socket_) {
        DeviceFinder::getInstance().removeIgnoredAddress(asio::ip::address_v4(localAddress_.load()));
        socket_.reset();
    }

    updates_.clear();
    masterTempo_.store(0.0);
    haveMasterTempo_.store(false);

    keepAliveBytes_[kDeviceNumberOffset] = 0;

    deliverLifecycleAnnouncement(false);
}

asio::ip::address_v4 VirtualCdj::getLocalAddress() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    if (!socket_) {
        return VirtualRekordbox::getInstance().getLocalAddress();
    }
    return asio::ip::address_v4(localAddress_.load());
}

asio::ip::address_v4 VirtualCdj::getBroadcastAddress() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    if (!socket_) {
        return VirtualRekordbox::getInstance().getBroadcastAddress();
    }
    return asio::ip::address_v4(broadcastAddress_.load());
}

void VirtualCdj::setUseStandardPlayerNumber(bool attempt) {
    useStandardPlayerNumber_.store(attempt);
}

bool VirtualCdj::getUseStandardPlayerNumber() const {
    return useStandardPlayerNumber_.load();
}

uint8_t VirtualCdj::getDeviceNumber() const {
    return keepAliveBytes_[kDeviceNumberOffset];
}

void VirtualCdj::setDeviceNumber(uint8_t number) {
    if (isRunning()) {
        throw std::runtime_error("Cannot change device number while running");
    }
    keepAliveBytes_[kDeviceNumberOffset] = number;
}

const std::string& VirtualCdj::getDeviceName() const {
    return deviceName_;
}

void VirtualCdj::setDeviceName(const std::string& name) {
    deviceName_ = name;
    std::fill(keepAliveBytes_.begin() + kDeviceNameOffset,
              keepAliveBytes_.begin() + kDeviceNameOffset + kDeviceNameLength,
              0);
    std::memcpy(keepAliveBytes_.data() + kDeviceNameOffset,
                deviceName_.data(),
                std::min(deviceName_.size(), kDeviceNameLength));
}

int VirtualCdj::getAnnounceInterval() const {
    return announceInterval_.load();
}

void VirtualCdj::setAnnounceInterval(int interval) {
    if (interval < 200 || interval > 2000) {
        throw std::invalid_argument("Interval must be between 200 and 2000");
    }
    announceInterval_.store(interval);
}

std::vector<std::shared_ptr<DeviceUpdate>> VirtualCdj::getLatestStatus() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::vector<std::shared_ptr<DeviceUpdate>> result;
    std::lock_guard<std::mutex> lock(updatesMutex_);
    result.reserve(updates_.size());
    for (const auto& entry : updates_) {
        result.push_back(entry.second);
    }
    return result;
}

std::shared_ptr<DeviceUpdate> VirtualCdj::getLatestStatusFor(const DeviceUpdate& device) const {
    std::lock_guard<std::mutex> lock(updatesMutex_);
    auto it = updates_.find(DeviceReference(device.getAddress(), device.getDeviceNumber()));
    if (it != updates_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<DeviceUpdate> VirtualCdj::getLatestStatusFor(const DeviceAnnouncement& device) const {
    std::lock_guard<std::mutex> lock(updatesMutex_);
    auto it = updates_.find(DeviceReference(device.getAddress(), device.getDeviceNumber()));
    if (it != updates_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<DeviceUpdate> VirtualCdj::getLatestStatusFor(int deviceNumber) const {
    std::lock_guard<std::mutex> lock(updatesMutex_);
    for (const auto& entry : updates_) {
        if (entry.second && entry.second->getDeviceNumber() == deviceNumber) {
            return entry.second;
        }
    }
    return nullptr;
}

std::shared_ptr<DeviceUpdate> VirtualCdj::getTempoMaster() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    if (master_.load()) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(updatesMutex_);
    return tempoMaster_;
}

double VirtualCdj::getTempoEpsilon() const {
    return tempoEpsilon_.load();
}

void VirtualCdj::setTempoEpsilon(double epsilon) {
    tempoEpsilon_.store(epsilon);
}

double VirtualCdj::getMasterTempo() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    return masterTempo_.load();
}

void VirtualCdj::addMasterListener(const MasterListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    masterListeners_.push_back(listener);
}

void VirtualCdj::removeMasterListener(const MasterListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    masterListeners_.erase(
        std::remove(masterListeners_.begin(), masterListeners_.end(), listener),
        masterListeners_.end());
}

void VirtualCdj::addUpdateListener(const DeviceUpdateListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    updateListeners_.push_back(listener);
}

void VirtualCdj::removeUpdateListener(const DeviceUpdateListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    updateListeners_.erase(
        std::remove(updateListeners_.begin(), updateListeners_.end(), listener),
        updateListeners_.end());
}

void VirtualCdj::addMediaDetailsListener(const MediaDetailsListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    mediaDetailsListeners_.push_back(listener);
}

void VirtualCdj::removeMediaDetailsListener(const MediaDetailsListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    mediaDetailsListeners_.erase(
        std::remove(mediaDetailsListeners_.begin(), mediaDetailsListeners_.end(), listener),
        mediaDetailsListeners_.end());
}

void VirtualCdj::processUpdate(const std::shared_ptr<DeviceUpdate>& update) {
    if (!update) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(updatesMutex_);
        updates_[DeviceReference(update->getAddress(), update->getDeviceNumber())] = update;
    }

    if (auto cdj = std::dynamic_pointer_cast<CdjStatus>(update)) {
        int syncNumber = cdj->getSyncNumber();
        if (syncNumber > largestSyncCounter_.load()) {
            largestSyncCounter_.store(syncNumber);
        }
    }

    if (update->isTempoMaster()) {
        auto yieldingTo = update->getDeviceMasterIsBeingYieldedTo();
        if (!yieldingTo) {
            if (master_.load()) {
                if (nextMaster_.load() == update->getDeviceNumber()) {
                    syncCounter_.store(largestSyncCounter_.load() + 1);
                }
            }
            master_.store(false);
            nextMaster_.store(0xff);
            setTempoMaster(update);
            if (update->getBpm() != 0xffff) {
                setMasterTempo(update->getEffectiveTempo());
            }
        } else if (*yieldingTo == getDeviceNumber()) {
            master_.store(true);
            masterYieldedFrom_.store(0);
            setTempoMaster(nullptr);
            setMasterTempo(getTempo());
        }
    } else {
        auto currentMaster = getTempoMaster();
        if (currentMaster && currentMaster->getAddress() == update->getAddress() &&
            currentMaster->getDeviceNumber() == update->getDeviceNumber()) {
            setTempoMaster(nullptr);
        }
    }

    deliverDeviceUpdate(update);
}

void VirtualCdj::processBeat(const Beat& beat) {
    if (!isRunning()) {
        return;
    }
    if (beat.isTempoMaster()) {
        setMasterTempo(beat.getEffectiveTempo());
        deliverBeatAnnouncement(beat);
    }
}

void VirtualCdj::sendMediaQuery(int deviceNumber, TrackSourceSlot slot) {
    auto announcement = DeviceFinder::getInstance().getLatestAnnouncementFrom(deviceNumber);
    if (!announcement) {
        throw std::runtime_error("Device not found on network");
    }
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }

    std::array<uint8_t, kMediaQueryPayload.size()> payload = kMediaQueryPayload;
    payload[2] = getDeviceNumber();
    payload[5] = keepAliveBytes_[kIpAddressOffset];
    payload[6] = keepAliveBytes_[kIpAddressOffset + 1];
    payload[7] = keepAliveBytes_[kIpAddressOffset + 2];
    payload[8] = keepAliveBytes_[kIpAddressOffset + 3];
    payload[12] = static_cast<uint8_t>(deviceNumber);
    payload[16] = static_cast<uint8_t>(slot);

    assembleAndSendPacket(PacketType::MEDIA_QUERY, payload, announcement->getAddress(), Ports::UPDATE);
}

void VirtualCdj::sendSyncModeCommand(int deviceNumber, bool synced) {
    auto target = getLatestStatusFor(deviceNumber);
    if (!target) {
        throw std::runtime_error("Device not found on network");
    }
    sendSyncModeCommand(*target, synced);
}

void VirtualCdj::sendSyncModeCommand(const DeviceUpdate& target, bool synced) {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::array<uint8_t, kSyncControlPayload.size()> payload = kSyncControlPayload;
    payload[2] = getDeviceNumber();
    payload[8] = getDeviceNumber();
    payload[12] = synced ? 0x10 : 0x20;
    assembleAndSendPacket(PacketType::SYNC_CONTROL, payload, target.getAddress(), Ports::BEAT);
}

void VirtualCdj::appointTempoMaster(int deviceNumber) {
    auto target = getLatestStatusFor(deviceNumber);
    if (!target) {
        throw std::runtime_error("Device not found on network");
    }
    appointTempoMaster(*target);
}

void VirtualCdj::appointTempoMaster(const DeviceUpdate& target) {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::array<uint8_t, kSyncControlPayload.size()> payload = kSyncControlPayload;
    payload[2] = getDeviceNumber();
    payload[8] = getDeviceNumber();
    payload[12] = 0x01;
    assembleAndSendPacket(PacketType::SYNC_CONTROL, payload, target.getAddress(), Ports::BEAT);
}

void VirtualCdj::sendFaderStartCommand(const std::set<int>& deviceNumbersToStart,
                                       const std::set<int>& deviceNumbersToStop) {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::array<uint8_t, kFaderStartPayload.size()> payload = kFaderStartPayload;
    payload[2] = getDeviceNumber();
    for (int i = 0; i < 4; ++i) {
        int channel = i + 1;
        if (deviceNumbersToStart.count(channel) > 0) {
            payload[i + 4] = 0;
        } else if (deviceNumbersToStop.count(channel) > 0) {
            payload[i + 4] = 1;
        } else {
            payload[i + 4] = 2;
        }
    }
    assembleAndSendPacket(PacketType::FADER_START_COMMAND, payload, getBroadcastAddress(), Ports::BEAT);
}

void VirtualCdj::sendOnAirCommand(const std::set<int>& deviceNumbersOnAir) {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::array<uint8_t, kChannelsOnAirPayload.size()> payload = kChannelsOnAirPayload;
    payload[2] = getDeviceNumber();
    for (int i = 0; i < 4; ++i) {
        if (deviceNumbersOnAir.count(i + 1) > 0) {
            payload[i + 4] = 1;
        }
    }
    assembleAndSendPacket(PacketType::CHANNELS_ON_AIR, payload, getBroadcastAddress(), Ports::BEAT);
}

void VirtualCdj::sendOnAirExtendedCommand(const std::set<int>& deviceNumbersOnAir) {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::array<uint8_t, kChannelsOnAirExtendedPayload.size()> payload = kChannelsOnAirExtendedPayload;
    payload[2] = getDeviceNumber();
    for (int i = 0; i < 4; ++i) {
        if (deviceNumbersOnAir.count(i + 1) > 0) {
            payload[i + 4] = 1;
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (deviceNumbersOnAir.count(i + 5) > 0) {
            payload[i + 9] = 1;
        }
    }
    assembleAndSendPacket(PacketType::CHANNELS_ON_AIR, payload, getBroadcastAddress(), Ports::BEAT);
}

void VirtualCdj::sendLoadTrackCommand(int targetPlayer,
                                      int rekordboxId,
                                      int sourcePlayer,
                                      TrackSourceSlot sourceSlot,
                                      TrackType sourceType) {
    auto target = getLatestStatusFor(targetPlayer);
    if (!target) {
        throw std::runtime_error("Device not found on network");
    }
    sendLoadTrackCommand(*target, rekordboxId, sourcePlayer, sourceSlot, sourceType);
}

void VirtualCdj::sendLoadTrackCommand(const DeviceUpdate& target,
                                      int rekordboxId,
                                      int sourcePlayer,
                                      TrackSourceSlot sourceSlot,
                                      TrackType sourceType) {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::array<uint8_t, kLoadTrackPayload.size()> payload = kLoadTrackPayload;
    payload[0x02] = getDeviceNumber();
    payload[0x05] = getDeviceNumber();
    payload[0x09] = static_cast<uint8_t>(sourcePlayer);
    payload[0x0a] = static_cast<uint8_t>(sourceSlot);
    payload[0x0b] = static_cast<uint8_t>(sourceType);
    payload[0x21] = static_cast<uint8_t>(target.getDeviceNumber() - 1);

    if (target.getDeviceName().starts_with("XDJ-XZ")) {
        payload[0x01] = 0x01;
        payload[0x2c] = 0x32;
        const auto devices = DeviceFinder::getInstance().getCurrentDevices();
        for (const auto& device : devices) {
            const auto number = static_cast<uint8_t>(device.getDeviceNumber());
            if (number >= 0x11 && number < 0x20) {
                payload[0x02] = number;
                payload[0x05] = number;
                break;
            }
        }
        if (payload[0x02] == getDeviceNumber()) {
            for (const auto& device : devices) {
                const auto number = static_cast<uint8_t>(device.getDeviceNumber());
                if (number >= 0x29 && number < 0x30) {
                    payload[0x02] = number;
                    payload[0x05] = number;
                    break;
                }
            }
        }
    }

    Util::numberToBytes(rekordboxId, payload.data(), payload.size(), 0x0d, 4);
    assembleAndSendPacket(PacketType::LOAD_TRACK_COMMAND, payload, target.getAddress(), Ports::UPDATE);
}

void VirtualCdj::sendLoadSettingsCommand(int targetPlayer, const PlayerSettings& settings) {
    auto target = getLatestStatusFor(targetPlayer);
    if (!target) {
        throw std::runtime_error("Device not found on network");
    }
    sendLoadSettingsCommand(*target, settings);
}

void VirtualCdj::sendLoadSettingsCommand(const DeviceUpdate& target, const PlayerSettings& settings) {
    if (!isRunning()) {
        throw std::runtime_error("VirtualCdj not running");
    }
    std::array<uint8_t, kLoadSettingsPayload.size()> payload = kLoadSettingsPayload;
    Util::setPayloadByte(payload, 0x20, getDeviceNumber());
    Util::setPayloadByte(payload, 0x21, static_cast<uint8_t>(target.getDeviceNumber()));
    settings.applyToPayload(payload);
    assembleAndSendPacket(PacketType::LOAD_SETTINGS_COMMAND, payload, target.getAddress(), Ports::UPDATE);
}

void VirtualCdj::setSendingStatus(bool send) {
    if (sendingStatus_.load() == send) {
        return;
    }

    if (send) {
        if (!isRunning()) {
            throw std::runtime_error("VirtualCdj not running");
        }
        if (getDeviceNumber() < 1 || getDeviceNumber() > 4) {
            throw std::runtime_error("Device number must be 1-4 to send status");
        }
        BeatFinder::getInstance().start();
        sendingStatus_.store(true);
        statusSenderThread_ = std::make_unique<std::thread>([this]() {
            while (sendingStatus_.load()) {
                sendStatus();
                std::this_thread::sleep_for(std::chrono::milliseconds(statusInterval_.load()));
            }
        });
        if (isSynced()) {
            if (syncMasterListener_) {
                addMasterListener(syncMasterListener_);
            }
        }
        if (isPlaying()) {
            beatSender_ = std::make_unique<BeatSender>(*this, metronome_);
        }
    } else {
        sendingStatus_.store(false);
        if (statusSenderThread_ && statusSenderThread_->joinable()) {
            statusSenderThread_->join();
        }
        statusSenderThread_.reset();
        beatSender_.reset();
        if (syncMasterListener_) {
            removeMasterListener(syncMasterListener_);
        }
    }
}

bool VirtualCdj::isSendingStatus() const {
    return sendingStatus_.load();
}

void VirtualCdj::setPlaying(bool playing) {
    if (playing_.load() == playing) {
        return;
    }
    playing_.store(playing);
    if (playing) {
        metronome_.jumpToBeat(whereStoppedBeat_.load());
        if (isSendingStatus()) {
            beatSender_ = std::make_unique<BeatSender>(*this, metronome_);
        }
    } else {
        beatSender_.reset();
        whereStoppedBeat_.store(metronome_.getSnapshot().getBeat());
    }
}

bool VirtualCdj::isPlaying() const {
    return playing_.load();
}

Snapshot VirtualCdj::getPlaybackPosition() const {
    if (playing_.load()) {
        return metronome_.getSnapshot();
    }
    Snapshot snapshot = metronome_.getSnapshot(metronome_.getTimeOfBeat(whereStoppedBeat_.load()));
    return snapshot;
}

void VirtualCdj::adjustPlaybackPosition(int ms) {
    if (ms == 0) {
        return;
    }
    metronome_.adjustStart(-ms);
    if (metronome_.getBeat() < 1) {
        metronome_.adjustStart(static_cast<int>(Metronome::beatsToMilliseconds(metronome_.getBeatsPerBar(), metronome_.getTempo())));
    }
    notifyBeatSenderOfChange();
}

void VirtualCdj::becomeTempoMaster() {
    if (!isSendingStatus()) {
        throw std::runtime_error("Must be sending status to become master");
    }
    auto currentMaster = getTempoMaster();
    if (currentMaster) {
        std::array<uint8_t, kMasterHandoffRequestPayload.size()> payload = kMasterHandoffRequestPayload;
        payload[2] = getDeviceNumber();
        payload[8] = getDeviceNumber();
        requestingMasterRoleFromPlayer_.store(currentMaster->getDeviceNumber());
        assembleAndSendPacket(PacketType::MASTER_HANDOFF_REQUEST, payload, currentMaster->getAddress(), Ports::BEAT);
    } else if (!master_.load()) {
        master_.store(true);
        setTempoMaster(nullptr);
        setMasterTempo(getTempo());
    }
}

bool VirtualCdj::isTempoMaster() const {
    return master_.load();
}

void VirtualCdj::setSynced(bool sync) {
    bool previous = synced_.load();
    if (previous != sync) {
        if (sync && isSendingStatus() && syncMasterListener_) {
            addMasterListener(syncMasterListener_);
        } else if (!sync && syncMasterListener_) {
            removeMasterListener(syncMasterListener_);
        }
    }
    synced_.store(sync);
    if (sync && !isTempoMaster()) {
        auto currentMaster = getTempoMaster();
        if (currentMaster) {
            setTempo(currentMaster->getEffectiveTempo());
        }
    }
    notifyBeatSenderOfChange();
}

bool VirtualCdj::isSynced() const {
    return synced_.load();
}

void VirtualCdj::setOnAir(bool audible) {
    onAir_.store(audible);
}

bool VirtualCdj::isOnAir() const {
    return onAir_.load();
}

void VirtualCdj::setTempo(double bpm) {
    tempo_.store(bpm);
    metronome_.setTempo(bpm);
    notifyBeatSenderOfChange();
}

double VirtualCdj::getTempo() const {
    return tempo_.load();
}

void VirtualCdj::jumpToBeat(int beat) {
    if (beat < 1) {
        beat = 1;
    }
    metronome_.jumpToBeat(beat);
    notifyBeatSenderOfChange();
}

int64_t VirtualCdj::sendBeat() {
    return sendBeat(getPlaybackPosition());
}

int64_t VirtualCdj::sendBeat(const Snapshot& snapshot) {
    std::array<uint8_t, kBeatPayload.size()> payload = kBeatPayload;
    payload[0x02] = getDeviceNumber();
    Util::numberToBytes(static_cast<int>(snapshot.getBeatInterval()), payload.data(), payload.size(), 0x05, 4);
    Util::numberToBytes(static_cast<int>(snapshot.getBeatInterval() * 2), payload.data(), payload.size(), 0x09, 4);
    Util::numberToBytes(static_cast<int>(snapshot.getBeatInterval() * 4), payload.data(), payload.size(), 0x11, 4);
    Util::numberToBytes(static_cast<int>(snapshot.getBeatInterval() * 8), payload.data(), payload.size(), 0x19, 4);

    int beatsLeft = 5 - snapshot.getBeatWithinBar();
    int nextBar = static_cast<int>(snapshot.getBeatInterval() * beatsLeft);
    Util::numberToBytes(nextBar, payload.data(), payload.size(), 0x0d, 4);
    Util::numberToBytes(nextBar + static_cast<int>(snapshot.getBarInterval()), payload.data(), payload.size(), 0x15, 4);
    Util::numberToBytes(static_cast<int>(std::round(snapshot.getTempo() * 100)), payload.data(), payload.size(), 0x3b, 2);
    payload[0x3d] = static_cast<uint8_t>(snapshot.getBeatWithinBar());
    payload[0x40] = getDeviceNumber();

    assembleAndSendPacket(PacketType::BEAT, payload, getBroadcastAddress(), Ports::BEAT);
    return snapshot.getBeat();
}

void VirtualCdj::handleSpecialAnnouncementPacket(PacketType kind,
                                                 const uint8_t* data,
                                                 size_t length,
                                                 const asio::ip::address_v4& sender) {
    switch (kind) {
        case PacketType::DEVICE_NUMBER_STAGE_1:
            break;
        case PacketType::DEVICE_NUMBER_STAGE_2:
            handleDeviceClaimPacket(data, length, 0x2e, sender);
            break;
        case PacketType::DEVICE_NUMBER_STAGE_3:
            handleDeviceClaimPacket(data, length, 0x24, sender);
            break;
        case PacketType::DEVICE_NUMBER_WILL_ASSIGN:
            if (claimingNumber_.load() != 0) {
                requestNumberFromMixer(sender);
            }
            break;
        case PacketType::DEVICE_NUMBER_ASSIGN:
            if (length > 0x24) {
                mixerAssigned_.store(data[0x24]);
            }
            break;
        case PacketType::DEVICE_NUMBER_ASSIGNMENT_FINISHED:
            mixerAssigned_.store(claimingNumber_.load());
            break;
        case PacketType::DEVICE_NUMBER_IN_USE:
            if (length > 0x24) {
                int defended = data[0x24];
                if (defended == claimingNumber_.load()) {
                    claimRejected_.store(true);
                } else if (isRunning() && defended == getDeviceNumber()) {
                    stop();
                }
            }
            break;
        default:
            break;
    }
}

void VirtualCdj::receiverLoop() {
    while (running_.load()) {
        try {
            socket_->non_blocking(true);
            asio::ip::udp::endpoint senderEndpoint;
            std::array<uint8_t, 512> buffer{};
            asio::error_code ec;
            size_t length = socket_->receive_from(asio::buffer(buffer), senderEndpoint, 0, ec);
            if (ec == asio::error::would_block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (ec) {
                continue;
            }
            auto type = Util::validateHeader(std::span<const uint8_t>(buffer.data(), length), Ports::UPDATE);
            if (!type) {
                continue;
            }
            if (*type == PacketType::MEDIA_RESPONSE) {
                try {
                    MediaDetails details(buffer.data(), length);
                    deliverMediaDetails(details);
                } catch (...) {
                }
                continue;
            }

            auto update = buildUpdate(buffer.data(), length, senderEndpoint.address().to_v4());
            if (update) {
                processUpdate(update);
            }
        } catch (...) {
        }
    }
}

void VirtualCdj::announcerLoop() {
    while (running_.load()) {
        sendAnnouncement();
    }
}

void VirtualCdj::sendAnnouncement() {
    if (!socket_) {
        return;
    }
    asio::ip::udp::endpoint target(asio::ip::address_v4(broadcastAddress_.load()), Ports::ANNOUNCEMENT);
    asio::error_code ec;
    socket_->send_to(asio::buffer(keepAliveBytes_), target, 0, ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(announceInterval_.load()));
}

std::shared_ptr<DeviceUpdate> VirtualCdj::buildUpdate(const uint8_t* data,
                                                      size_t length,
                                                      const asio::ip::address_v4& sender) {
    auto type = Util::validateHeader(std::span<const uint8_t>(data, length), Ports::UPDATE);
    if (!type) {
        return nullptr;
    }

    switch (*type) {
        case PacketType::MIXER_STATUS:
            if (length < MixerStatus::PACKET_SIZE) {
                return nullptr;
            }
            return std::make_shared<MixerStatus>(data, MixerStatus::PACKET_SIZE, sender);
        case PacketType::CDJ_STATUS:
            if (length < CdjStatus::MIN_PACKET_SIZE) {
                return nullptr;
            }
            return std::make_shared<CdjStatus>(data, length, sender);
        default:
            return nullptr;
    }
}

void VirtualCdj::deliverMasterChangedAnnouncement(const std::shared_ptr<DeviceUpdate>& update) {
    std::vector<MasterListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = masterListeners_;
    }
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->masterChanged(update.get());
        } catch (...) {
        }
    }
}

void VirtualCdj::deliverTempoChangedAnnouncement(double tempo) {
    std::vector<MasterListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = masterListeners_;
    }
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->tempoChanged(tempo);
        } catch (...) {
        }
    }
}

void VirtualCdj::deliverBeatAnnouncement(const Beat& beat) {
    std::vector<MasterListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = masterListeners_;
    }
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->newBeat(beat);
        } catch (...) {
        }
    }
}

void VirtualCdj::deliverDeviceUpdate(const std::shared_ptr<DeviceUpdate>& update) {
    std::vector<DeviceUpdateListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = updateListeners_;
    }
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->received(*update);
        } catch (...) {
        }
    }
}

void VirtualCdj::deliverMediaDetails(const MediaDetails& details) {
    std::vector<MediaDetailsListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = mediaDetailsListeners_;
    }
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->detailsAvailable(details);
        } catch (...) {
        }
    }
}

void VirtualCdj::setTempoMaster(const std::shared_ptr<DeviceUpdate>& update) {
    std::shared_ptr<DeviceUpdate> oldMaster;
    {
        std::lock_guard<std::mutex> lock(updatesMutex_);
        oldMaster = tempoMaster_;
        tempoMaster_ = update;
    }
    const bool changed = (!oldMaster && update) ||
                         (oldMaster && !update) ||
                         (oldMaster && update &&
                          (oldMaster->getAddress() != update->getAddress() ||
                           oldMaster->getDeviceNumber() != update->getDeviceNumber()));
    if (changed) {
        deliverMasterChangedAnnouncement(update);
    }
}

void VirtualCdj::setMasterTempo(double tempo) {
    const double epsilon = tempoEpsilon_.load();
    const double current = masterTempo_.load();
    if (!haveMasterTempo_.load() || std::abs(current - tempo) > epsilon) {
        masterTempo_.store(tempo);
        haveMasterTempo_.store(true);
        if (synced_.load() && !isTempoMaster()) {
            metronome_.setTempo(tempo);
            notifyBeatSenderOfChange();
        }
        deliverTempoChangedAnnouncement(tempo);
    }
}

bool VirtualCdj::selfAssignDeviceNumber() {
    const auto firstDeviceTime = DeviceFinder::getInstance().getFirstDeviceTime();
    if (firstDeviceTime.time_since_epoch().count() == 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - firstDeviceTime).count();
    while (elapsed < kSelfAssignmentWatchPeriodMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kSelfAssignmentWatchPeriodMs - elapsed));
        now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - firstDeviceTime).count();
    }

    if (claimingNumber_.load() == 0) {
        if (!getUseStandardPlayerNumber()) {
            claimingNumber_.store(6);
        }
    }

    std::set<int> numbersUsed;
    for (const auto& device : DeviceFinder::getInstance().getCurrentDevices()) {
        numbersUsed.insert(device.getDeviceNumber());
    }

    int startingNumber = claimingNumber_.load() + 1;
    for (int number = startingNumber; number < 16; ++number) {
        if (numbersUsed.find(number) == numbersUsed.end()) {
            claimingNumber_.store(number);
            return true;
        }
    }
    return false;
}

bool VirtualCdj::claimDeviceNumber() {
    claimRejected_.store(false);
    int desired = getDeviceNumber();
    if (desired == 0) {
        if (!selfAssignDeviceNumber()) {
            return false;
        }
    } else {
        claimingNumber_.store(desired);
    }

    bool claimed = false;
    while (!claimed) {
        if (claimingNumber_.load() == 0) {
            return false;
        }

        const asio::ip::address_v4 broadcastAddr(broadcastAddress_.load());

        std::array<uint8_t, kClaimStage1.size()> stage1 = kClaimStage1;
        std::memcpy(stage1.data() + kDeviceNameOffset, deviceName_.data(), std::min(deviceName_.size(), kDeviceNameLength));
        std::copy(keepAliveBytes_.begin() + kMacAddressOffset, keepAliveBytes_.begin() + kMacAddressOffset + 6, stage1.begin() + 0x26);
        bool retry = false;
        for (int i = 1; i <= 3; ++i) {
            stage1[0x24] = static_cast<uint8_t>(i);
            sendRawPacket(stage1, broadcastAddr, Ports::ANNOUNCEMENT);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            if (claimRejected_.load()) {
                retry = true;
                break;
            }
        }
        if (retry) {
            claimRejected_.store(false);
            claimingNumber_.store(0);
            if (!selfAssignDeviceNumber()) {
                return false;
            }
            continue;
        }

        std::array<uint8_t, kClaimStage2.size()> stage2 = kClaimStage2;
        std::memcpy(stage2.data() + kDeviceNameOffset, deviceName_.data(), std::min(deviceName_.size(), kDeviceNameLength));
        stage2[0x24] = keepAliveBytes_[kIpAddressOffset];
        stage2[0x25] = keepAliveBytes_[kIpAddressOffset + 1];
        stage2[0x26] = keepAliveBytes_[kIpAddressOffset + 2];
        stage2[0x27] = keepAliveBytes_[kIpAddressOffset + 3];
        std::copy(keepAliveBytes_.begin() + kMacAddressOffset, keepAliveBytes_.begin() + kMacAddressOffset + 6, stage2.begin() + 0x28);
        stage2[0x2e] = static_cast<uint8_t>(claimingNumber_.load());
        stage2[0x31] = (getDeviceNumber() == 0) ? 1 : 2;
        retry = false;
        for (int i = 1; i <= 3; ++i) {
            stage2[0x2f] = static_cast<uint8_t>(i);
            sendRawPacket(stage2, broadcastAddr, Ports::ANNOUNCEMENT);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            if (claimRejected_.load()) {
                retry = true;
                break;
            }
        }
        if (retry) {
            claimRejected_.store(false);
            claimingNumber_.store(0);
            if (!selfAssignDeviceNumber()) {
                return false;
            }
            continue;
        }

        if (mixerAssigned_.load() != 0) {
            claimingNumber_.store(mixerAssigned_.load());
        }

        std::array<uint8_t, kClaimStage3.size()> stage3 = kClaimStage3;
        std::memcpy(stage3.data() + kDeviceNameOffset, deviceName_.data(), std::min(deviceName_.size(), kDeviceNameLength));
        stage3[0x24] = static_cast<uint8_t>(claimingNumber_.load());
        retry = false;
        for (int i = 1; i <= 3; ++i) {
            stage3[0x25] = static_cast<uint8_t>(i);
            sendRawPacket(stage3, broadcastAddr, Ports::ANNOUNCEMENT);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            if (claimRejected_.load()) {
                retry = true;
                break;
            }
        }
        if (retry) {
            claimRejected_.store(false);
            claimingNumber_.store(0);
            if (!selfAssignDeviceNumber()) {
                return false;
            }
            continue;
        }

        claimed = true;
    }

    keepAliveBytes_[kDeviceNumberOffset] = static_cast<uint8_t>(claimingNumber_.load());
    claimingNumber_.store(0);
    return true;
}

void VirtualCdj::requestNumberFromMixer(const asio::ip::address_v4& mixerAddress) {
    std::array<uint8_t, kAssignmentRequestBytes.size()> payload = kAssignmentRequestBytes;
    std::copy(keepAliveBytes_.begin() + kMacAddressOffset, keepAliveBytes_.begin() + kMacAddressOffset + 6, payload.begin() + 0x28);
    payload[0x31] = (keepAliveBytes_[kDeviceNumberOffset] == 0) ? 1 : 2;
    sendRawPacket(payload, mixerAddress, Ports::ANNOUNCEMENT);
}

void VirtualCdj::defendDeviceNumber(const asio::ip::address_v4& invaderAddress) {
    std::array<uint8_t, kDeviceNumberDefenseBytes.size()> payload = kDeviceNumberDefenseBytes;
    payload[0x24] = keepAliveBytes_[kDeviceNumberOffset];
    sendRawPacket(payload, invaderAddress, Ports::ANNOUNCEMENT);
}

void VirtualCdj::handleDeviceClaimPacket(const uint8_t* data, size_t length, int deviceOffset,
                                         const asio::ip::address_v4& sender) {
    if (length <= static_cast<size_t>(deviceOffset)) {
        return;
    }
    if (isRunning() && getDeviceNumber() == data[deviceOffset]) {
        defendDeviceNumber(sender);
    }
}

void VirtualCdj::assembleAndSendPacket(PacketType type,
                                       std::span<const uint8_t> payload,
                                       const asio::ip::address_v4& target,
                                       uint16_t port) {
    if (!socket_) {
        return;
    }
    const std::span<const uint8_t> nameSpan(keepAliveBytes_.data() + kDeviceNameOffset, kDeviceNameLength);
    auto packet = Util::buildPacket(type, nameSpan, payload);
    asio::ip::udp::endpoint endpoint(target, port);
    asio::error_code ec;
    socket_->send_to(asio::buffer(packet), endpoint, 0, ec);
}

void VirtualCdj::sendRawPacket(std::span<const uint8_t> packet,
                               const asio::ip::address_v4& target,
                               uint16_t port) {
    if (!socket_) {
        return;
    }
    asio::ip::udp::endpoint endpoint(target, port);
    asio::error_code ec;
    socket_->send_to(asio::buffer(packet.data(), packet.size()), endpoint, 0, ec);
}

Snapshot VirtualCdj::avoidBeatPacket() {
    Snapshot playState = getPlaybackPosition();
    double distance = playState.distanceFromBeat();
    while (playing_.load() &&
           (((distance < 0.0) && (std::abs(distance) <= BeatSender::kSleepThresholdMs)) ||
            ((distance >= 0.0) && (distance <= (BeatSender::kBeatThresholdMs + 1))))) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        playState = getPlaybackPosition();
        distance = playState.distanceFromBeat();
    }
    return playState;
}

void VirtualCdj::sendStatus() {
    Snapshot playState = avoidBeatPacket();
    bool playing = playing_.load();
    std::array<uint8_t, kStatusPayload.size()> payload = kStatusPayload;
    payload[0x02] = getDeviceNumber();
    payload[0x05] = payload[0x02];
    payload[0x08] = static_cast<uint8_t>(playing ? 1 : 0);
    payload[0x09] = payload[0x02];
    payload[0x5c] = static_cast<uint8_t>(playing ? 3 : 5);
    Util::numberToBytes(syncCounter_.load(), payload.data(), payload.size(), 0x65, 4);
    payload[0x6a] = static_cast<uint8_t>(0x84 + (playing ? 0x40 : 0) + (master_.load() ? 0x20 : 0) +
                                        (synced_.load() ? 0x10 : 0) + (onAir_.load() ? 0x08 : 0));
    payload[0x6c] = static_cast<uint8_t>(playing ? 0x7a : 0x7e);
    Util::numberToBytes(static_cast<int>(std::round(getTempo() * 100)), payload.data(), payload.size(), 0x73, 2);
    payload[0x7e] = static_cast<uint8_t>(playing ? 9 : 1);
    payload[0x7f] = static_cast<uint8_t>(master_.load() ? 1 : 0);
    payload[0x80] = static_cast<uint8_t>(nextMaster_.load());
    Util::numberToBytes(static_cast<int>(playState.getBeat()), payload.data(), payload.size(), 0x81, 4);
    payload[0x87] = static_cast<uint8_t>(playState.getBeatWithinBar());
    Util::numberToBytes(packetCounter_.fetch_add(1) + 1, payload.data(), payload.size(), 0xa9, 4);

    for (const auto& device : DeviceFinder::getInstance().getCurrentDevices()) {
        assembleAndSendPacket(PacketType::CDJ_STATUS, payload, device.getAddress(), Ports::UPDATE);
    }
}

void VirtualCdj::notifyBeatSenderOfChange() {
    if (beatSender_) {
        beatSender_->timelineChanged();
    }
}

} // namespace beatlink
