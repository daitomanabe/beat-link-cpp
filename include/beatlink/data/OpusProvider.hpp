#pragma once

#include <atomic>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace beatlink::data {

class OpusProvider {
public:
    struct DeviceSqlRekordboxIdAndSlot {
        int rekordboxId{0};
        int usbSlot{0};
    };

    static OpusProvider& getInstance() {
        static OpusProvider instance;
        return instance;
    }

    bool isRunning() const { return running_.load(); }
    bool usingDeviceLibraryPlus() const { return usingDeviceLibraryPlus_.load(); }

    void start() { running_.store(true); }
    void stop() { running_.store(false); }

    void setDatabaseKey(const std::string& key) {
        usingDeviceLibraryPlus_.store(!key.empty());
        databaseKey_ = key;
    }

    const std::string& getDatabaseKey() const { return databaseKey_; }

    void pollAndSendMediaDetails(int /*player*/) const {}

    std::optional<DeviceSqlRekordboxIdAndSlot> getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
        std::span<const uint8_t> /*pssiData*/,
        int /*rekordboxIdFromOpus*/) const
    {
        return std::nullopt;
    }

    std::optional<DeviceSqlRekordboxIdAndSlot> getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
        const std::vector<uint8_t>& pssiData,
        int rekordboxIdFromOpus) const
    {
        return getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
            std::span<const uint8_t>(pssiData.data(), pssiData.size()),
            rekordboxIdFromOpus);
    }

private:
    OpusProvider() = default;

    std::atomic<bool> running_{false};
    std::atomic<bool> usingDeviceLibraryPlus_{false};
    std::string databaseKey_;
};

} // namespace beatlink::data
