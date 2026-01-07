#pragma once

#include "Client.hpp"

#include "beatlink/DeviceAnnouncement.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/DeviceFinder.hpp"
#include "beatlink/LifecycleParticipant.hpp"
#include "beatlink/VirtualCdj.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace beatlink::dbserver {

class ConnectionManager : public beatlink::LifecycleParticipant {
public:
    static constexpr int DEFAULT_SOCKET_TIMEOUT = 10000;

    static ConnectionManager& getInstance();

    bool isRunning() const override;

    void start();
    void stop();

    void setIdleLimit(int seconds);
    int getIdleLimit() const;

    void setSocketTimeout(int timeoutMs);
    int getSocketTimeout() const;

    int getPlayerDBServerPort(int player);

    template <typename Task>
    auto invokeWithClientSession(int targetPlayer, Task&& task, const std::string& description)
        -> decltype(task(std::declval<Client&>())) {
        if (!isRunning()) {
            throw std::runtime_error("ConnectionManager is not running, aborting " + description);
        }

        auto client = allocateClient(targetPlayer, description);
        using Result = decltype(task(std::declval<Client&>()));
        try {
            if constexpr (std::is_void_v<Result>) {
                task(*client);
                freeClient(client);
                return;
            } else {
                Result result = task(*client);
                freeClient(client);
                return result;
            }
        } catch (...) {
            freeClient(client);
            throw;
        }
    }

    std::string toString() const;

private:
    ConnectionManager();

    std::shared_ptr<Client> allocateClient(int targetPlayer, const std::string& description);
    void freeClient(const std::shared_ptr<Client>& client);
    void closeClient(int targetPlayer);

    void requestPlayerDBServerPort(const beatlink::DeviceAnnouncement& announcement);
    int chooseAskingPlayerNumber(const beatlink::DeviceAnnouncement& targetPlayer);

    void closeIdleClients();

    struct ClientRecord {
        std::shared_ptr<Client> client;
        int useCount{0};
        std::chrono::steady_clock::time_point lastUsed;
    };

    mutable std::mutex clientsMutex_;
    std::unordered_map<int, ClientRecord> openClients_;

    mutable std::mutex dbServerMutex_;
    std::unordered_map<uint32_t, int> dbServerPorts_;

    std::atomic<int> idleLimit_{1};
    std::atomic<int> socketTimeout_{DEFAULT_SOCKET_TIMEOUT};

    std::atomic<bool> running_{false};

    beatlink::DeviceAnnouncementListenerPtr announcementListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;

    std::thread idleCloserThread_;

    std::mutex activeQueryMutex_;
    std::unordered_map<uint32_t, bool> activeQueries_;
};

} // namespace beatlink::dbserver
