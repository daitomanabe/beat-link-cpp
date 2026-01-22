#pragma once

#include "DeviceUpdate.hpp"
#include "Util.hpp"

#include <fmt/format.h>
#include <optional>
#include "Span.hpp"

namespace beatlink {

/**
 * Represents a status update sent by a mixer on a DJ Link network.
 * Ported from org.deepsymmetry.beatlink.MixerStatus
 */
class MixerStatus : public DeviceUpdate {
public:
    // Status flag bits (same as CdjStatus)
    static constexpr int STATUS_FLAGS = 0x27;
    static constexpr int BPM_SYNC_FLAG = 0x02;
    static constexpr int ON_AIR_FLAG = 0x08;
    static constexpr int SYNCED_FLAG = 0x10;
    static constexpr int MASTER_FLAG = 0x20;
    static constexpr int PLAYING_FLAG = 0x40;

    /**
     * Expected packet size for mixer status.
     */
    static constexpr size_t PACKET_SIZE = 0x38; // 56 bytes

    /**
     * Factory function to create a MixerStatus from raw packet data.
     * Returns std::nullopt if packet size is invalid.
     *
     * @param data the packet bytes as span
     * @param senderAddress the IP address of the sender
     * @return std::optional<MixerStatus> - nullopt if invalid
     */
    [[nodiscard]] static std::optional<MixerStatus> create(
        std::span<const uint8_t> data,
        const asio::ip::address_v4& senderAddress
    ) noexcept {
        if (!validatePacketSize(data.size(), PACKET_SIZE)) {
            return std::nullopt;
        }
        return MixerStatus(data, senderAddress, NoValidateTag{});
    }

    /**
     * Construct from raw packet data.
     * @deprecated Use create() factory function for exception-safe construction
     */
    MixerStatus(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress)
        : DeviceUpdate(data, length, senderAddress, "Mixer status", PACKET_SIZE)
    {
        initParsing(data);
    }

private:
    /**
     * Private constructor for factory function (noexcept).
     */
    MixerStatus(std::span<const uint8_t> data, const asio::ip::address_v4& senderAddress, NoValidateTag) noexcept
        : DeviceUpdate(data, senderAddress, DEVICE_NUMBER_OFFSET, NoValidateTag{})
    {
        initParsing(data.data());
    }

    void initParsing(const uint8_t* data) noexcept {
        // Parse BPM (offset 0x2e, 2 bytes)
        bpm_ = static_cast<int>(Util::bytesToNumber(data, 0x2e, 2));

        // Parse beat within bar (offset 0x37)
        beatWithinBar_ = data[0x37];
    }

public:

    // Mixers report pitch at 0%, so pitch is always neutral
    int getPitch() const override { return static_cast<int>(Util::NEUTRAL_PITCH); }

    int getBpm() const override { return bpm_; }

    double getEffectiveTempo() const override {
        return bpm_ / 100.0;
    }

    int getBeatWithinBar() const override { return beatWithinBar_; }

    /**
     * Mixer beat-within-bar is not meaningful (not synced to track data).
     */
    bool isBeatWithinBarMeaningful() const override {
        return false;
    }

    uint8_t getStatusFlags() const {
        return packetBytes_.size() > STATUS_FLAGS ? packetBytes_[STATUS_FLAGS] : 0;
    }

    bool isTempoMaster() const override {
        return (getStatusFlags() & MASTER_FLAG) != 0;
    }

    bool isSynced() const override {
        return (getStatusFlags() & SYNCED_FLAG) != 0;
    }

    std::optional<int> getDeviceMasterIsBeingYieldedTo() const override {
        return std::nullopt; // Mixers don't yield master
    }

    std::string toString() const override {
        return fmt::format(
            "MixerStatus: Device {}, name: {}, master: {}, BPM: {:.2f}, beat: {}",
            deviceNumber_,
            deviceName_,
            isTempoMaster() ? "true" : "false",
            bpm_ / 100.0,
            beatWithinBar_);
    }

private:
    int bpm_;
    int beatWithinBar_;
};

} // namespace beatlink
