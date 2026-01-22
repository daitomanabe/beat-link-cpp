#include "beatlink/VirtualRekordbox.hpp"

#include "beatlink/MixerStatus.hpp"
#include "beatlink/VirtualCdj.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <optional>
#include <beatlink/Span.hpp>
#include <unordered_set>

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

constexpr int kMetadataTypePssi = 10;
constexpr int kSelfAssignmentWatchPeriodMs = 4000;

constexpr std::array<uint8_t, 0x36> kRekordboxKeepAliveBytesTemplate = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x06, 0x00, 0x72, 0x65, 0x6b, 0x6f,
    0x72, 0x64, 0x62, 0x6f, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x00, 0x36, 0x17, 0x01, 0x18, 0x3e, 0xef, 0xda, 0x5b, 0xca, 0xc0, 0xa8, 0x02, 0x0b,
    0x04, 0x01, 0x00, 0x00, 0x04, 0x08
};

constexpr std::array<uint8_t, 0x128> kRekordboxLightingRequestStatusTemplate = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x11, 0x72, 0x65, 0x6b, 0x6f, 0x72,
    0x64, 0x62, 0x6f, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x17, 0x01, 0x04, 0x17, 0x01, 0x00, 0x00, 0x00, 0x6d, 0x00, 0x61, 0x00, 0x63, 0x00, 0x62,
    0x00, 0x6f, 0x00, 0x6f, 0x00, 0x6b, 0x00, 0x20, 0x00, 0x70, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

constexpr std::array<uint8_t, 0x2c> kRequestPssiBytes = {
    0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c, 0x55, 0x72, 0x65, 0x6b, 0x6f, 0x72,
    0x64, 0x62, 0x6f, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x17, 0x00, 0x08, 0x36, 0x00, 0x00, 0x00, 0x0a, 0x02, 0x03, 0x01
};

struct InterfaceMatch {
    std::string name;
    asio::ip::address_v4 address;
    asio::ip::address_v4 netmask;
    asio::ip::address_v4 broadcast;
    std::array<uint8_t, 6> mac{};
    int prefixLength{0};
};

int prefixLengthFromNetmask(uint32_t netmask) {
    int length = 0;
    uint32_t mask = ntohl(netmask);
    while (mask & 0x80000000U) {
        ++length;
        mask <<= 1U;
    }
    return length;
}

std::vector<InterfaceMatch> findMatchingInterfaces(const asio::ip::address_v4& deviceAddress) {
    std::vector<InterfaceMatch> matches;
#if defined(__APPLE__) || defined(__linux__)
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0 || !ifaddr) {
        return matches;
    }

    for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        auto* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
        auto* netmask = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
        if (!sin || !netmask) {
            continue;
        }
        uint32_t addr = ntohl(sin->sin_addr.s_addr);
        uint32_t mask = ntohl(netmask->sin_addr.s_addr);
        int prefixLength = prefixLengthFromNetmask(netmask->sin_addr.s_addr);
        if (!Util::sameNetwork(prefixLength, addr, deviceAddress.to_uint())) {
            continue;
        }

        InterfaceMatch match;
        match.name = ifa->ifa_name;
        match.address = asio::ip::address_v4(addr);
        match.netmask = asio::ip::address_v4(mask);
        match.prefixLength = prefixLength;
        if (ifa->ifa_broadaddr && ifa->ifa_broadaddr->sa_family == AF_INET) {
            auto* broad = reinterpret_cast<sockaddr_in*>(ifa->ifa_broadaddr);
            match.broadcast = asio::ip::address_v4(ntohl(broad->sin_addr.s_addr));
        } else {
            match.broadcast = asio::ip::address_v4((addr & mask) | (~mask));
        }
        matches.push_back(match);
    }

    for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
#if defined(__APPLE__)
        if (ifa->ifa_addr->sa_family != AF_LINK) {
            continue;
        }
        auto* sdl = reinterpret_cast<sockaddr_dl*>(ifa->ifa_addr);
        if (!sdl || sdl->sdl_alen < 6) {
            continue;
        }
        const auto* mac = reinterpret_cast<const uint8_t*>(LLADDR(sdl));
#elif defined(__linux__)
        if (ifa->ifa_addr->sa_family != AF_PACKET) {
            continue;
        }
        auto* sll = reinterpret_cast<sockaddr_ll*>(ifa->ifa_addr);
        if (!sll || sll->sll_halen < 6) {
            continue;
        }
        const auto* mac = reinterpret_cast<const uint8_t*>(sll->sll_addr);
#endif
        for (auto& match : matches) {
            if (match.name == ifa->ifa_name) {
                std::copy(mac, mac + 6, match.mac.begin());
            }
        }
    }

    freeifaddrs(ifaddr);
#endif
    return matches;
}

} // namespace

VirtualRekordbox& VirtualRekordbox::getInstance() {
    static VirtualRekordbox instance;
    return instance;
}

VirtualRekordbox::VirtualRekordbox() {
    std::copy(kRekordboxKeepAliveBytesTemplate.begin(),
              kRekordboxKeepAliveBytesTemplate.end(),
              rekordboxKeepAliveBytes_.begin());
    std::copy(kRekordboxLightingRequestStatusTemplate.begin(),
              kRekordboxLightingRequestStatusTemplate.end(),
              rekordboxLightingRequestStatusBytes_.begin());

    deviceFinderLifecycleListener_ = std::make_shared<LifecycleCallbacks>(
        [](LifecycleParticipant&) {},
        [](LifecycleParticipant&) {
            if (VirtualRekordbox::getInstance().isRunning()) {
                VirtualRekordbox::getInstance().stop();
            }
        });
}

VirtualRekordbox::~VirtualRekordbox() {
    stop();
}

bool VirtualRekordbox::isRunning() const {
    return running_.load() && socket_;
}

asio::ip::address_v4 VirtualRekordbox::getLocalAddress() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualRekordbox not running");
    }
    return asio::ip::address_v4(localAddress_.load());
}

asio::ip::address_v4 VirtualRekordbox::getBroadcastAddress() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualRekordbox not running");
    }
    return asio::ip::address_v4(broadcastAddress_.load());
}

uint8_t VirtualRekordbox::getDeviceNumber() const {
    return rekordboxKeepAliveBytes_[kDeviceNumberOffset];
}

void VirtualRekordbox::setDeviceNumber(uint8_t number) {
    if (isRunning()) {
        throw std::runtime_error("Can't change device number once started");
    }
    rekordboxKeepAliveBytes_[kDeviceNumberOffset] = number;
}

int VirtualRekordbox::getAnnounceInterval() const {
    return announceInterval_.load();
}

void VirtualRekordbox::setAnnounceInterval(int interval) {
    if (interval < 200 || interval > 2000) {
        throw std::invalid_argument("Interval must be between 200 and 2000");
    }
    announceInterval_.store(interval);
}

std::string VirtualRekordbox::getDeviceName() {
    return Util::extractString(kRekordboxKeepAliveBytesTemplate.data(), kDeviceNameOffset, kDeviceNameLength);
}

void VirtualRekordbox::requestPssi() {
    if (!isRunning()) {
        return;
    }
    auto devices = DeviceFinder::getInstance().getCurrentDevices();
    if (devices.empty()) {
        return;
    }
    asio::ip::udp::endpoint target(devices.front().getAddress(), Ports::UPDATE);
    asio::error_code ec;
    socket_->send_to(asio::buffer(kRequestPssiBytes.data(), kRequestPssiBytes.size()), target, 0, ec);
}

void VirtualRekordbox::clearPlayerCaches(int usbSlotNumber) {
    std::lock_guard<std::mutex> lock(opusMutex_);
    playerTrackSourceSlots_.erase(usbSlotNumber);
}

std::optional<data::SlotReference> VirtualRekordbox::findMatchedTrackSourceSlotForPlayer(int player) const {
    std::lock_guard<std::mutex> lock(opusMutex_);
    auto it = playerTrackSourceSlots_.find(player);
    if (it == playerTrackSourceSlots_.end()) {
        return std::nullopt;
    }
    return it->second;
}

int VirtualRekordbox::findDeviceSqlRekordboxIdForPlayer(int player) const {
    std::lock_guard<std::mutex> lock(opusMutex_);
    auto it = playerToDeviceSqlRekordboxId_.find(player);
    if (it == playerToDeviceSqlRekordboxId_.end()) {
        return 0;
    }
    return it->second;
}

std::optional<asio::ip::address_v4> VirtualRekordbox::getMatchedAddress() const {
    return matchedAddress_;
}

std::vector<std::string> VirtualRekordbox::getMatchingInterfaces() const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualRekordbox not running");
    }
    return matchingInterfaces_;
}

void VirtualRekordbox::sendRekordboxLightingPacket() {
    if (!isRunning()) {
        return;
    }
    asio::ip::udp::endpoint target(getBroadcastAddress(), Ports::UPDATE);
    asio::error_code ec;
    socket_->send_to(asio::buffer(rekordboxLightingRequestStatusBytes_.data(), rekordboxLightingRequestStatusBytes_.size()),
                     target, 0, ec);
}

void VirtualRekordbox::sendRekordboxAnnouncement() {
    if (!isRunning()) {
        return;
    }
    asio::ip::udp::endpoint target(getBroadcastAddress(), Ports::ANNOUNCEMENT);
    asio::error_code ec;
    socket_->send_to(asio::buffer(rekordboxKeepAliveBytes_.data(), rekordboxKeepAliveBytes_.size()),
                     target, 0, ec);
}

bool VirtualRekordbox::start() {
    if (isRunning()) {
        return true;
    }

    DeviceFinder::getInstance().addLifecycleListener(deviceFinderLifecycleListener_);

    DeviceFinder::getInstance().start();
    for (int i = 0; DeviceFinder::getInstance().getCurrentDevices().empty() && i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (DeviceFinder::getInstance().getCurrentDevices().empty()) {
        return false;
    }

    return createVirtualRekordbox();
}

void VirtualRekordbox::stop() {
    if (!isRunning()) {
        return;
    }

    running_.store(false);

    if (socket_) {
        asio::error_code ec;
        socket_->close(ec);
    }

    if (receiverThread_.joinable() && receiverThread_.get_id() != std::this_thread::get_id()) {
        receiverThread_.join();
    }
    if (announcerThread_.joinable() && announcerThread_.get_id() != std::this_thread::get_id()) {
        announcerThread_.join();
    }

    if (socket_) {
        DeviceFinder::getInstance().removeIgnoredAddress(socket_->local_endpoint().address().to_v4());
    }
    if (broadcastAddress_.load() != 0) {
        DeviceFinder::getInstance().removeIgnoredAddress(asio::ip::address_v4(broadcastAddress_.load()));
    }

    socket_.reset();
    broadcastAddress_.store(0);
    localAddress_.store(0);

    {
        std::lock_guard<std::mutex> lock(updatesMutex_);
        updates_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(opusMutex_);
        playerTrackSourceSlots_.clear();
        playerToDeviceSqlRekordboxId_.clear();
        lastValidStatusFlagBytes_.clear();
        previousRawRekordboxIds_.clear();
        playerPacketTrackers_.clear();
    }

    setDeviceNumber(0);
    deliverLifecycleAnnouncement(false);
}

std::shared_ptr<DeviceUpdate> VirtualRekordbox::getLatestStatusFor(int deviceNumber) const {
    if (!isRunning()) {
        throw std::runtime_error("VirtualRekordbox not running");
    }
    std::lock_guard<std::mutex> lock(updatesMutex_);
    for (const auto& entry : updates_) {
        if (entry.second && entry.second->getDeviceNumber() == deviceNumber) {
            return entry.second;
        }
    }
    return nullptr;
}

void VirtualRekordbox::addUpdateListener(const DeviceUpdateListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    updateListeners_.push_back(listener);
}

void VirtualRekordbox::removeUpdateListener(const DeviceUpdateListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    updateListeners_.erase(
        std::remove(updateListeners_.begin(), updateListeners_.end(), listener),
        updateListeners_.end());
}

std::vector<DeviceUpdateListenerPtr> VirtualRekordbox::getUpdateListeners() const {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    return updateListeners_;
}

std::string VirtualRekordbox::toString() const {
    return "VirtualRekordbox[number:" + std::to_string(getDeviceNumber()) +
           ", name:" + getDeviceName() + "]";
}

bool VirtualRekordbox::PacketTracker::receivePacket(const std::vector<uint8_t>& packetData,
                                                    int packetNumber,
                                                    int totalPackets) {
    data.insert(data.end(), packetData.begin(), packetData.end());
    return packetNumber == totalPackets;
}

void VirtualRekordbox::PacketTracker::resetForNewPacketStream() {
    data.clear();
}

std::vector<uint8_t> VirtualRekordbox::PacketTracker::getDataAsBytesAndTrimTrailingZeros() const {
    std::vector<uint8_t> result = data;
    while (!result.empty() && result.back() == 0) {
        result.pop_back();
    }
    return result;
}

bool VirtualRekordbox::createVirtualRekordbox() {
    data::OpusProvider::getInstance().start();

    auto devices = DeviceFinder::getInstance().getCurrentDevices();
    if (devices.empty()) {
        return false;
    }

    const auto& announcement = devices.front();

    auto matches = findMatchingInterfaces(announcement.getAddress());
    if (matches.empty()) {
        return false;
    }

    matchingInterfaces_.clear();
    for (const auto& match : matches) {
        matchingInterfaces_.push_back(match.name);
    }

    const auto& selected = matches.front();
    matchedAddress_ = selected.address;
    matchedPrefixLength_.store(selected.prefixLength);
    broadcastAddress_.store(selected.broadcast.to_uint());
    localAddress_.store(selected.address.to_uint());

    socket_ = std::make_unique<asio::ip::udp::socket>(ioContext_);
    socket_->open(asio::ip::udp::v4());
    socket_->set_option(asio::socket_base::reuse_address(true));
    socket_->bind(asio::ip::udp::endpoint(selected.address, Ports::UPDATE));

    std::copy(selected.mac.begin(), selected.mac.end(), rekordboxKeepAliveBytes_.begin() + kMacAddressOffset);
    std::copy(selected.mac.begin(), selected.mac.end(),
              rekordboxLightingRequestStatusBytes_.begin() + kMacAddressOffset);

    auto localBytes = selected.address.to_bytes();
    rekordboxKeepAliveBytes_[kIpAddressOffset] = localBytes[0];
    rekordboxKeepAliveBytes_[kIpAddressOffset + 1] = localBytes[1];
    rekordboxKeepAliveBytes_[kIpAddressOffset + 2] = localBytes[2];
    rekordboxKeepAliveBytes_[kIpAddressOffset + 3] = localBytes[3];
    rekordboxLightingRequestStatusBytes_[kIpAddressOffset] = localBytes[0];
    rekordboxLightingRequestStatusBytes_[kIpAddressOffset + 1] = localBytes[1];
    rekordboxLightingRequestStatusBytes_[kIpAddressOffset + 2] = localBytes[2];
    rekordboxLightingRequestStatusBytes_[kIpAddressOffset + 3] = localBytes[3];

    DeviceFinder::getInstance().addIgnoredAddress(selected.broadcast);
    DeviceFinder::getInstance().addIgnoredAddress(selected.address);

    if (!selfAssignDeviceNumber()) {
        DeviceFinder::getInstance().removeIgnoredAddress(selected.address);
        socket_->close();
        socket_.reset();
        return false;
    }

    running_.store(true);

    receiverThread_ = std::thread(&VirtualRekordbox::receiverLoop, this);
    announcerThread_ = std::thread(&VirtualRekordbox::announcerLoop, this);

    deliverLifecycleAnnouncement(true);
    return true;
}

bool VirtualRekordbox::selfAssignDeviceNumber() {
    const auto started = DeviceFinder::getInstance().getFirstDeviceTime();
    if (started.time_since_epoch().count() != 0) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - started).count();
        if (elapsed < kSelfAssignmentWatchPeriodMs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kSelfAssignmentWatchPeriodMs - elapsed));
        }
    }

    std::unordered_set<int> numbersUsed;
    for (const auto& device : DeviceFinder::getInstance().getCurrentDevices()) {
        numbersUsed.insert(device.getDeviceNumber());
    }

    if (numbersUsed.find(getDeviceNumber()) == numbersUsed.end()) {
        return true;
    }

    for (int candidate = 0x13; candidate < 0x28; ++candidate) {
        if (numbersUsed.find(candidate) == numbersUsed.end()) {
            setDeviceNumber(static_cast<uint8_t>(candidate));
            return true;
        }
    }

    return false;
}

void VirtualRekordbox::receiverLoop() {
    while (running_.load()) {
        try {
            socket_->non_blocking(true);
            asio::ip::udp::endpoint senderEndpoint;
            std::array<uint8_t, 1420> buffer{};
            asio::error_code ec;
            size_t length = socket_->receive_from(asio::buffer(buffer), senderEndpoint, 0, ec);
            if (ec == asio::error::would_block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (ec) {
                if (running_.load()) {
                    DeviceFinder::getInstance().flush();
                    stop();
                }
                continue;
            }
            if (senderEndpoint.address() == socket_->local_endpoint().address()) {
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

void VirtualRekordbox::announcerLoop() {
    while (running_.load()) {
        sendAnnouncements();
    }
}

void VirtualRekordbox::sendAnnouncements() {
    try {
        sendRekordboxAnnouncement();
        sendRekordboxLightingPacket();
        std::this_thread::sleep_for(std::chrono::milliseconds(announceInterval_.load()));
    } catch (...) {
        DeviceFinder::getInstance().flush();
        stop();
    }
}

std::shared_ptr<DeviceUpdate> VirtualRekordbox::buildUpdate(const uint8_t* data,
                                                            size_t length,
                                                            const asio::ip::address_v4& sender) {
    auto kind = Util::validateHeader(std::span<const uint8_t>(data, length), Ports::UPDATE);
    if (!kind) {
        return nullptr;
    }

    switch (*kind) {
        case PacketType::MIXER_STATUS: {
            auto status = MixerStatus::create(std::span<const uint8_t>(data, length), sender);
            if (status) {
                return std::make_shared<MixerStatus>(std::move(*status));
            }
            return nullptr;
        }
        case PacketType::CDJ_STATUS: {
            if (length < CdjStatus::MIN_PACKET_SIZE) {
                return nullptr;
            }
            std::vector<uint8_t> packetCopy(data, data + length);
            const uint8_t reportedStatusFlags = packetCopy[CdjStatus::STATUS_FLAGS];
            const uint8_t rawDeviceNumber = packetCopy[DeviceUpdate::DEVICE_NUMBER_OFFSET];
            if (reportedStatusFlags == 0) {
                std::lock_guard<std::mutex> lock(opusMutex_);
                auto it = lastValidStatusFlagBytes_.find(rawDeviceNumber);
                if (it != lastValidStatusFlagBytes_.end()) {
                    packetCopy[CdjStatus::STATUS_FLAGS] = it->second;
                } else {
                    return nullptr;
                }
            } else {
                std::lock_guard<std::mutex> lock(opusMutex_);
                lastValidStatusFlagBytes_[rawDeviceNumber] = reportedStatusFlags;
            }

            int rawRekordboxId = static_cast<int>(Util::bytesToNumber(packetCopy.data(), 0x2c, 4));
            auto maybeStatus = CdjStatus::create(std::span<const uint8_t>(packetCopy), sender);
            if (!maybeStatus) {
                return nullptr;
            }
            auto status = std::make_shared<CdjStatus>(std::move(*maybeStatus));

            const int deviceNumber = status->getDeviceNumber();
            int previousId = 0;
            {
                std::lock_guard<std::mutex> lock(opusMutex_);
                auto it = previousRawRekordboxIds_.find(deviceNumber);
                if (it != previousRawRekordboxIds_.end()) {
                    previousId = it->second;
                }
                previousRawRekordboxIds_[deviceNumber] = rawRekordboxId;
            }

            if (previousId != rawRekordboxId) {
                std::lock_guard<std::mutex> lock(opusMutex_);
                playerTrackSourceSlots_.erase(deviceNumber);
                playerToDeviceSqlRekordboxId_.erase(deviceNumber);
                if (rawRekordboxId != 0) {
                    requestPssi();
                }
            }

            return status;
        }
        case PacketType::DEVICE_REKORDBOX_LIGHTING_HELLO: {
            auto status = CdjStatus::create(std::span<const uint8_t>(data, length), sender);
            if (status) {
                return std::make_shared<CdjStatus>(std::move(*status));
            }
            return nullptr;
        }
        case PacketType::OPUS_METADATA: {
            if (length <= 0x34) {
                return nullptr;
            }
            const auto playerNumber = Util::translateOpusPlayerNumbers(data[0x21]);
            const int rekordboxIdFromOpus = static_cast<int>(Util::bytesToNumber(data, 0x28, 4));
            std::vector<uint8_t> binaryData(data + 0x34, data + length);

            PacketTracker tracker;
            {
                std::lock_guard<std::mutex> lock(opusMutex_);
                tracker = playerPacketTrackers_[playerNumber];
            }

            if (data[0x25] == kMetadataTypePssi) {
                const int totalPackets = data[0x33] - 1;
                const int packetNumber = data[0x31];
                bool dataComplete = tracker.receivePacket(binaryData, packetNumber, totalPackets);

                if (dataComplete) {
                    auto pssiData = tracker.getDataAsBytesAndTrimTrailingZeros();
                    tracker.resetForNewPacketStream();
                    auto match = data::OpusProvider::getInstance().getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
                        pssiData, rekordboxIdFromOpus);
                    if (match) {
                        std::lock_guard<std::mutex> lock(opusMutex_);
                        playerToDeviceSqlRekordboxId_[playerNumber] = match->rekordboxId;
                        playerTrackSourceSlots_.insert_or_assign(
                            playerNumber,
                            data::SlotReference::getSlotReference(match->usbSlot, TrackSourceSlot::USB_SLOT));
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(opusMutex_);
                playerPacketTrackers_[playerNumber] = tracker;
            }
            return nullptr;
        }
        default:
            return nullptr;
    }
}

void VirtualRekordbox::processUpdate(const std::shared_ptr<DeviceUpdate>& update) {
    if (!update) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(updatesMutex_);
        updates_[DeviceReference(update->getAddress(), update->getDeviceNumber())] = update;
    }

    if (VirtualCdj::getInstance().isProxyingForVirtualRekordbox()) {
        VirtualCdj::getInstance().processUpdate(update);
    }

    deliverDeviceUpdate(update);
}

void VirtualRekordbox::deliverDeviceUpdate(const std::shared_ptr<DeviceUpdate>& update) {
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

} // namespace beatlink
