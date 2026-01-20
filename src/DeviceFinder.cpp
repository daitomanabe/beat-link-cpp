#include "beatlink/DeviceFinder.hpp"
#include "beatlink/VirtualCdj.hpp"
#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace beatlink {

DeviceFinder::DeviceFinder() {
}

DeviceFinder::~DeviceFinder() {
    stop();
}

bool DeviceFinder::start() {
    if (running_.load()) {
        return true;
    }

    try {
        socket_ = std::make_unique<asio::ip::udp::socket>(ioContext_);
        socket_->open(asio::ip::udp::v4());
        socket_->set_option(asio::socket_base::reuse_address(true));
        // Enable SO_REUSEPORT to allow sharing port with Rekordbox on same machine
        #ifdef SO_REUSEPORT
        int reuse = 1;
        setsockopt(socket_->native_handle(), SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
        #endif
        socket_->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), Ports::ANNOUNCEMENT));

        startTime_ = std::chrono::steady_clock::now();
        firstDeviceTime_ = std::chrono::steady_clock::time_point{};
        running_.store(true);

        receiverThread_ = std::thread(&DeviceFinder::receiverLoop, this);
        deliverLifecycleAnnouncement(true);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "DeviceFinder: Failed to start: " << e.what() << std::endl;
        socket_.reset();
        return false;
    }
}

void DeviceFinder::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (socket_) {
        asio::error_code ec;
        socket_->close(ec);
    }

    if (receiverThread_.joinable()) {
        receiverThread_.join();
    }

    socket_.reset();

    flush();

    deliverLifecycleAnnouncement(false);
}

void DeviceFinder::flush() {
    std::vector<DeviceAnnouncement> lastDevices;
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        for (const auto& pair : devices_) {
            lastDevices.push_back(pair.second);
        }
        devices_.clear();
        firstDeviceTime_ = std::chrono::steady_clock::time_point{};
    }

    limit3PlayersSeen_.store(false);

    for (const auto& device : lastDevices) {
        deliverLostAnnouncement(device);
    }
}

void DeviceFinder::receiverLoop() {
    while (running_.load()) {
        try {
            // Set timeout
            socket_->non_blocking(true);

            asio::error_code ec;
            size_t length = socket_->receive_from(
                asio::buffer(receiveBuffer_), senderEndpoint_, 0, ec);

            if (ec == asio::error::would_block) {
                // No data available, sleep briefly and check for expired devices
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                expireDevices();
                continue;
            }

            if (ec) {
                if (running_.load()) {
                    std::cerr << "DeviceFinder: Receive error: " << ec.message() << std::endl;
                }
                continue;
            }

            // Check if address is ignored
            auto senderAddr = senderEndpoint_.address().to_v4();
            if (isAddressIgnored(senderAddr)) {
                continue;
            }

            // Process the packet
            processPacket(receiveBuffer_.data(), length, senderAddr);

            // Expire old devices
            expireDevices();

        } catch (const std::exception& e) {
            if (running_.load()) {
                std::cerr << "DeviceFinder: Exception in receiver: " << e.what() << std::endl;
            }
        }
    }
}

void DeviceFinder::processPacket(const uint8_t* data, size_t length, const asio::ip::address_v4& sender) {
    auto type = Util::validateHeader(std::span<const uint8_t>(data, length), Ports::ANNOUNCEMENT);
    if (!type) {
        return;
    }

    // Both DEVICE_HELLO and DEVICE_KEEP_ALIVE contain device announcements
    if (*type == PacketType::DEVICE_KEEP_ALIVE || *type == PacketType::DEVICE_HELLO) {
        auto announcement = DeviceAnnouncement::create(std::span<const uint8_t>(data, length), sender);
        if (!announcement) {
            std::cerr << "DeviceFinder: Failed to parse announcement packet" << std::endl;
            return;
        }

        if (announcement->isOpusQuad()) {
            createAndProcessOpusAnnouncements(data, length, sender);
        } else {
            processAnnouncement(*announcement);
        }
        return;
    }

    // Delegate special device-number packets to the VirtualCdj.
    switch (*type) {
        case PacketType::DEVICE_NUMBER_STAGE_1:
        case PacketType::DEVICE_NUMBER_STAGE_2:
        case PacketType::DEVICE_NUMBER_STAGE_3:
        case PacketType::DEVICE_NUMBER_WILL_ASSIGN:
        case PacketType::DEVICE_NUMBER_ASSIGN:
        case PacketType::DEVICE_NUMBER_ASSIGNMENT_FINISHED:
        case PacketType::DEVICE_NUMBER_IN_USE:
            VirtualCdj::getInstance().handleSpecialAnnouncementPacket(*type, data, length, sender);
            break;
        default:
            break;
    }
}

void DeviceFinder::processAnnouncement(const DeviceAnnouncement& announcement) {
    bool isNew = isDeviceNew(announcement);
    updateDevices(announcement);

    if (isNew) {
        deliverFoundAnnouncement(announcement);
    }
}

void DeviceFinder::createAndProcessOpusAnnouncements(const uint8_t* data, size_t length, const asio::ip::address_v4& sender) {
    for (int i = 1; i <= 4; ++i) {
        auto announcement = DeviceAnnouncement::create(std::span<const uint8_t>(data, length), sender, i);
        if (!announcement) {
            std::cerr << "DeviceFinder: Failed to parse Opus announcement" << std::endl;
            continue;
        }

        bool isNew = isDeviceNew(*announcement);
        updateDevices(*announcement);

        if (isNew) {
            deliverFoundAnnouncement(*announcement);
        }
    }
}

bool DeviceFinder::isDeviceNew(const DeviceAnnouncement& announcement) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    auto ref = DeviceReference::fromAnnouncement(announcement);
    return devices_.find(ref) == devices_.end();
}

void DeviceFinder::updateDevices(const DeviceAnnouncement& announcement) {
    std::lock_guard<std::mutex> lock(devicesMutex_);

    // Update first device time
    if (firstDeviceTime_.time_since_epoch().count() == 0) {
        firstDeviceTime_ = std::chrono::steady_clock::now();
    }

    auto ref = DeviceReference::fromAnnouncement(announcement);
    devices_.insert_or_assign(ref, announcement);
}

void DeviceFinder::expireDevices() {
    auto now = std::chrono::steady_clock::now();
    std::vector<DeviceAnnouncement> expired;

    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        for (auto it = devices_.begin(); it != devices_.end(); ) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.getTimestamp()).count();

            if (age > MAXIMUM_AGE) {
                expired.push_back(it->second);
                it = devices_.erase(it);
            } else {
                ++it;
            }
        }

        if (devices_.empty()) {
            firstDeviceTime_ = std::chrono::steady_clock::time_point{};
        }
    }

    for (const auto& device : expired) {
        deliverLostAnnouncement(device);
    }
}

std::vector<DeviceAnnouncement> DeviceFinder::getCurrentDevices() {
    expireDevices();

    std::lock_guard<std::mutex> lock(devicesMutex_);
    std::vector<DeviceAnnouncement> result;
    result.reserve(devices_.size());
    for (const auto& pair : devices_) {
        result.push_back(pair.second);
    }
    return result;
}

bool DeviceFinder::isDeviceMetadataLimited(const DeviceAnnouncement& announcement) const {
    static const std::unordered_set<std::string> kMetadataFlexibleDevices = {"CDJ-3000", "XDJ-AZ"};
    return announcement.getDeviceNumber() < 7 &&
           kMetadataFlexibleDevices.find(announcement.getDeviceName()) == kMetadataFlexibleDevices.end();
}

std::optional<DeviceAnnouncement> DeviceFinder::getLatestAnnouncementFrom(int deviceNumber) {
    auto devices = getCurrentDevices();
    for (const auto& device : devices) {
        if (device.getDeviceNumber() == deviceNumber) {
            return device;
        }
    }
    return std::nullopt;
}

void DeviceFinder::addIgnoredAddress(const asio::ip::address_v4& address) {
    std::lock_guard<std::mutex> lock(ignoredMutex_);
    ignoredAddresses_.insert(address.to_uint());
}

void DeviceFinder::removeIgnoredAddress(const asio::ip::address_v4& address) {
    std::lock_guard<std::mutex> lock(ignoredMutex_);
    ignoredAddresses_.erase(address.to_uint());
}

bool DeviceFinder::isAddressIgnored(const asio::ip::address_v4& address) const {
    std::lock_guard<std::mutex> lock(ignoredMutex_);
    return ignoredAddresses_.find(address.to_uint()) != ignoredAddresses_.end();
}

void DeviceFinder::addDeviceAnnouncementListener(const DeviceAnnouncementListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    announcementListeners_.push_back(listener);
}

void DeviceFinder::removeDeviceAnnouncementListener(const DeviceAnnouncementListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    announcementListeners_.erase(
        std::remove(announcementListeners_.begin(), announcementListeners_.end(), listener),
        announcementListeners_.end());
}

void DeviceFinder::addDeviceFoundListener(DeviceAnnouncementCallback callback) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    foundListeners_.push_back(std::move(callback));
}

void DeviceFinder::addDeviceLostListener(DeviceAnnouncementCallback callback) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    lostListeners_.push_back(std::move(callback));
}

void DeviceFinder::clearListeners() {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    foundListeners_.clear();
    lostListeners_.clear();
    announcementListeners_.clear();
}

void DeviceFinder::deliverFoundAnnouncement(const DeviceAnnouncement& announcement) {
    if (isDeviceMetadataLimited(announcement)) {
        limit3PlayersSeen_.store(true);
    }

    std::vector<DeviceAnnouncementCallback> listeners;
    std::vector<DeviceAnnouncementListenerPtr> announcementListeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = foundListeners_;
        announcementListeners = announcementListeners_;
    }

    for (const auto& listener : listeners) {
        try {
            listener(announcement);
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Exception in found listener: " << e.what() << std::endl;
        }
    }

    for (const auto& listener : announcementListeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->deviceFound(announcement);
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Exception in announcement listener: " << e.what() << std::endl;
        }
    }
}

void DeviceFinder::deliverLostAnnouncement(const DeviceAnnouncement& announcement) {
    if (isDeviceMetadataLimited(announcement)) {
        auto devices = getCurrentDevices();
        bool anyLimited = false;
        for (const auto& device : devices) {
            if (isDeviceMetadataLimited(device)) {
                anyLimited = true;
                break;
            }
        }
        if (!anyLimited) {
            limit3PlayersSeen_.store(false);
        }
    }

    std::vector<DeviceAnnouncementCallback> listeners;
    std::vector<DeviceAnnouncementListenerPtr> announcementListeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = lostListeners_;
        announcementListeners = announcementListeners_;
    }

    for (const auto& listener : listeners) {
        try {
            listener(announcement);
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Exception in lost listener: " << e.what() << std::endl;
        }
    }

    for (const auto& listener : announcementListeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->deviceLost(announcement);
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Exception in announcement listener: " << e.what() << std::endl;
        }
    }
}

} // namespace beatlink
