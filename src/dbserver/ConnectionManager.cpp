#include "beatlink/dbserver/ConnectionManager.hpp"
#include "beatlink/dbserver/DataReader.hpp"

#include "beatlink/CdjStatus.hpp"
#include "beatlink/DeviceUpdate.hpp"
#include "beatlink/Util.hpp"

#include <array>
#include <asio.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

namespace beatlink::dbserver {

namespace {
constexpr int DB_SERVER_QUERY_PORT = 12523;
constexpr std::array<uint8_t, 19> DB_SERVER_QUERY_PACKET = {
    0x00, 0x00, 0x00, 0x0f,
    0x52, 0x65, 0x6d, 0x6f, 0x74, 0x65, 0x44, 0x42, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
    0x00
};
} // namespace

ConnectionManager& ConnectionManager::getInstance() {
    static ConnectionManager instance;
    return instance;
}

ConnectionManager::ConnectionManager() = default;

bool ConnectionManager::isRunning() const {
    return running_.load();
}

void ConnectionManager::setIdleLimit(int seconds) {
    if (seconds < 0) {
        throw std::invalid_argument("seconds cannot be negative");
    }
    idleLimit_.store(seconds);
}

int ConnectionManager::getIdleLimit() const {
    return idleLimit_.load();
}

void ConnectionManager::setSocketTimeout(int timeoutMs) {
    socketTimeout_.store(timeoutMs);
}

int ConnectionManager::getSocketTimeout() const {
    return socketTimeout_.load();
}

int ConnectionManager::getPlayerDBServerPort(int player) {
    ensureRunning();
    auto announcement = beatlink::DeviceFinder::getInstance().getLatestAnnouncementFrom(player);
    if (!announcement) {
        return -1;
    }
    const auto addr = announcement->getAddress().to_uint();
    std::lock_guard<std::mutex> lock(dbServerMutex_);
    auto it = dbServerPorts_.find(addr);
    if (it == dbServerPorts_.end()) {
        return -1;
    }
    return it->second;
}

std::shared_ptr<Client> ConnectionManager::allocateClient(int targetPlayer, const std::string& description) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = openClients_.find(targetPlayer);
    if (it != openClients_.end()) {
        it->second.useCount += 1;
        return it->second.client;
    }

    auto announcement = beatlink::DeviceFinder::getInstance().getLatestAnnouncementFrom(targetPlayer);
    if (!announcement) {
        throw std::runtime_error("Player " + std::to_string(targetPlayer) + " could not be found " + description);
    }
    const int dbServerPort = getPlayerDBServerPort(targetPlayer);
    if (dbServerPort < 0) {
        throw std::runtime_error("Player " + std::to_string(targetPlayer) + " does not have a db server " + description);
    }

    const int posingAsPlayerNumber = chooseAskingPlayerNumber(*announcement);
    auto client = std::make_shared<Client>(announcement->getAddress(), dbServerPort, targetPlayer, posingAsPlayerNumber,
                                           std::chrono::milliseconds(socketTimeout_.load()));

    ClientRecord record;
    record.client = client;
    record.useCount = 1;
    record.lastUsed = std::chrono::steady_clock::now();
    openClients_.insert({targetPlayer, record});
    return client;
}

void ConnectionManager::closeClient(int targetPlayer) {
    auto it = openClients_.find(targetPlayer);
    if (it == openClients_.end()) {
        return;
    }
    if (it->second.client) {
        it->second.client->close();
    }
    openClients_.erase(it);
}

void ConnectionManager::freeClient(const std::shared_ptr<Client>& client) {
    if (!client) {
        return;
    }
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = openClients_.find(client->targetPlayer());
    if (it == openClients_.end()) {
        return;
    }
    if (it->second.useCount > 0) {
        it->second.useCount -= 1;
        it->second.lastUsed = std::chrono::steady_clock::now();
        if (it->second.useCount == 0 && idleLimit_.load() == 0) {
            closeClient(client->targetPlayer());
        }
    }
}

void ConnectionManager::closeIdleClients() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    if (openClients_.empty()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto idleSeconds = idleLimit_.load();
    for (auto it = openClients_.begin(); it != openClients_.end();) {
        if (it->second.useCount < 1) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastUsed).count();
            if (elapsed >= idleSeconds * 1000LL) {
                if (it->second.client) {
                    it->second.client->close();
                }
                it = openClients_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void ConnectionManager::requestPlayerDBServerPort(const beatlink::DeviceAnnouncement& announcement) {
    const auto addressKey = announcement.getAddress().to_uint();
    struct QueryGuard {
        std::mutex& mutex;
        std::unordered_map<uint32_t, bool>& map;
        uint32_t key;
        ~QueryGuard() {
            std::lock_guard<std::mutex> lock(mutex);
            map.erase(key);
        }
    } guard{activeQueryMutex_, activeQueries_, addressKey};

    for (int tries = 0; tries < 4; ++tries) {
        if (tries > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 * tries));
        }

        try {
            asio::io_context io;
            asio::ip::tcp::socket socket(io);
            asio::ip::tcp::endpoint endpoint(announcement.getAddress(), DB_SERVER_QUERY_PORT);
            socket.connect(endpoint);

            asio::write(socket, asio::buffer(DB_SERVER_QUERY_PACKET.data(), DB_SERVER_QUERY_PACKET.size()));

            std::array<uint8_t, 2> response{};
            DataReader reader(socket, std::chrono::milliseconds(socketTimeout_.load()));
            reader.readFully(response.data(), response.size());

            const int portReturned = static_cast<int>(beatlink::Util::bytesToNumber(response.data(), 0, 2));
            if (isRunning()) {
                std::lock_guard<std::mutex> lock(dbServerMutex_);
                dbServerPorts_[addressKey] = portReturned;
            }
            return;
        } catch (const std::exception&) {
            continue;
        }
    }
}

int ConnectionManager::chooseAskingPlayerNumber(const beatlink::DeviceAnnouncement& targetPlayer) {
    const int fakeDevice = beatlink::VirtualCdj::getInstance().getDeviceNumber();
    if (fakeDevice > 4 && !beatlink::DeviceFinder::getInstance().isDeviceMetadataLimited(targetPlayer)) {
        return targetPlayer.getDeviceNumber();
    }

    if (targetPlayer.getDeviceNumber() > 15 || (fakeDevice >= 1 && fakeDevice <= 4)) {
        return fakeDevice;
    }

    for (const auto& candidate : beatlink::DeviceFinder::getInstance().getCurrentDevices()) {
        const int realDevice = candidate.getDeviceNumber();
        if (realDevice != targetPlayer.getDeviceNumber() && realDevice >= 1 && realDevice <= 4) {
            auto lastUpdate = beatlink::VirtualCdj::getInstance().getLatestStatusFor(realDevice);
            auto cdjStatus = std::dynamic_pointer_cast<beatlink::CdjStatus>(lastUpdate);
            if (cdjStatus && cdjStatus->getTrackSourcePlayer() != targetPlayer.getDeviceNumber()) {
                return candidate.getDeviceNumber();
            }
        }
    }
    throw std::runtime_error("No player number available to query player " + std::to_string(targetPlayer.getDeviceNumber()));
}

void ConnectionManager::start() {
    if (isRunning()) {
        return;
    }

    running_.store(true);

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });

    announcementListener_ = std::make_shared<beatlink::DeviceAnnouncementCallbacks>(
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (beatlink::VirtualCdj::getInstance().inOpusQuadCompatibilityMode()) {
                return;
            }
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            const auto addressKey = announcement.getAddress().to_uint();
            {
                std::lock_guard<std::mutex> lock(activeQueryMutex_);
                if (activeQueries_.find(addressKey) != activeQueries_.end()) {
                    return;
                }
                activeQueries_[addressKey] = true;
            }

            std::thread([this, announcement]() {
                requestPlayerDBServerPort(announcement);
            }).detach();
        },
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            std::lock_guard<std::mutex> lock(dbServerMutex_);
            dbServerPorts_.erase(announcement.getAddress().to_uint());
        });

    beatlink::DeviceFinder::getInstance().addLifecycleListener(lifecycleListener_);
    beatlink::DeviceFinder::getInstance().addDeviceAnnouncementListener(announcementListener_);
    beatlink::DeviceFinder::getInstance().start();

    for (const auto& device : beatlink::DeviceFinder::getInstance().getCurrentDevices()) {
        if (announcementListener_) {
            announcementListener_->deviceFound(device);
        }
    }

    idleCloserThread_ = std::thread([this]() {
        while (isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            closeIdleClients();
        }
    });

    deliverLifecycleAnnouncement(true);
}

void ConnectionManager::stop() {
    if (!isRunning()) {
        return;
    }

    running_.store(false);
    beatlink::DeviceFinder::getInstance().removeDeviceAnnouncementListener(announcementListener_);
    beatlink::DeviceFinder::getInstance().removeLifecycleListener(lifecycleListener_);

    {
        std::lock_guard<std::mutex> lock(dbServerMutex_);
        dbServerPorts_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(activeQueryMutex_);
        activeQueries_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& entry : openClients_) {
            if (entry.second.client) {
                entry.second.client->close();
            }
        }
        openClients_.clear();
    }

    if (idleCloserThread_.joinable()) {
        idleCloserThread_.join();
    }

    deliverLifecycleAnnouncement(false);
}

std::string ConnectionManager::toString() const {
    std::ostringstream oss;
    oss << "ConnectionManager[running:" << isRunning() << ", openClients:" << openClients_.size()
        << ", idleLimit:" << idleLimit_.load() << "]";
    return oss.str();
}

} // namespace beatlink::dbserver
