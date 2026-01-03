#include "beatlink/DeviceFinder.hpp"
#include <iostream>

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

    // Notify lost for all devices
    std::vector<DeviceAnnouncement> lastDevices;
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        for (const auto& pair : devices_) {
            lastDevices.push_back(pair.second);
        }
        devices_.clear();
    }

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
    // Validate header
    if (!Util::validateHeader(data, length)) {
        return;
    }

    // Get packet type
    uint8_t packetType = Util::getPacketType(data, length);
    auto type = PacketTypes::lookup(Ports::ANNOUNCEMENT, packetType);

    if (!type) {
        return;
    }

    // Both DEVICE_HELLO and DEVICE_KEEP_ALIVE contain device announcements
    if (*type == PacketType::DEVICE_KEEP_ALIVE || *type == PacketType::DEVICE_HELLO) {
        if (length < DeviceAnnouncement::PACKET_SIZE) {
            std::cerr << "DeviceFinder: Ignoring too-short announcement packet" << std::endl;
            return;
        }

        try {
            DeviceAnnouncement announcement(data, DeviceAnnouncement::PACKET_SIZE, sender);

            if (announcement.isOpusQuad()) {
                createAndProcessOpusAnnouncements(data, length, sender);
            } else {
                processAnnouncement(announcement);
            }
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Error processing announcement: " << e.what() << std::endl;
        }
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
        try {
            DeviceAnnouncement announcement(data, DeviceAnnouncement::PACKET_SIZE, sender, i);
            bool isNew = isDeviceNew(announcement);
            updateDevices(announcement);

            if (isNew) {
                deliverFoundAnnouncement(announcement);
            }
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Error processing Opus announcement: " << e.what() << std::endl;
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
}

void DeviceFinder::deliverFoundAnnouncement(const DeviceAnnouncement& announcement) {
    std::vector<DeviceAnnouncementCallback> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = foundListeners_;
    }

    for (const auto& listener : listeners) {
        try {
            listener(announcement);
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Exception in found listener: " << e.what() << std::endl;
        }
    }
}

void DeviceFinder::deliverLostAnnouncement(const DeviceAnnouncement& announcement) {
    std::vector<DeviceAnnouncementCallback> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = lostListeners_;
    }

    for (const auto& listener : listeners) {
        try {
            listener(announcement);
        } catch (const std::exception& e) {
            std::cerr << "DeviceFinder: Exception in lost listener: " << e.what() << std::endl;
        }
    }
}

} // namespace beatlink
