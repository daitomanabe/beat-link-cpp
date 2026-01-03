#pragma once

#include "DeviceUpdate.hpp"
#include "Util.hpp"

namespace beatlink {

/**
 * A device update that announces the start of a new beat on a DJ Link network.
 * Ported from org.deepsymmetry.beatlink.Beat
 */
class Beat : public DeviceUpdate {
public:
    /**
     * The expected size of a beat packet.
     */
    static constexpr size_t PACKET_SIZE = 0x60; // 96 bytes

    /**
     * Construct from raw packet data.
     */
    Beat(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress)
        : DeviceUpdate(data, length, senderAddress, "Beat announcement", PACKET_SIZE)
        , pitch_(static_cast<int>(Util::bytesToNumber(data, 0x55, 3)))
        , bpm_(static_cast<int>(Util::bytesToNumber(data, 0x5a, 2)))
    {
    }

    /**
     * Get the device pitch at the time of the beat.
     * Range 0-2097152 (0 = stopped, 1048576 = normal, 2097152 = 2x).
     */
    int getPitch() const override { return pitch_; }

    /**
     * Get the track BPM * 100 (e.g., 12050 = 120.50 BPM).
     */
    int getBpm() const override { return bpm_; }

    /**
     * Get the position within a measure (1-4, where 1 is the downbeat).
     */
    int getBeatWithinBar() const override {
        return packetBytes_[0x5c];
    }

    /**
     * Get the time until the next beat in milliseconds (at normal pitch).
     * Returns 0xffffffff if track ends before that beat.
     */
    int64_t getNextBeat() const {
        return Util::bytesToNumber(packetBytes_.data(), 0x24, 4);
    }

    /**
     * Get the time until the second upcoming beat in milliseconds.
     */
    int64_t getSecondBeat() const {
        return Util::bytesToNumber(packetBytes_.data(), 0x28, 4);
    }

    /**
     * Get the time until the next bar (downbeat) in milliseconds.
     */
    int64_t getNextBar() const {
        return Util::bytesToNumber(packetBytes_.data(), 0x2c, 4);
    }

    /**
     * Get the time until the fourth beat in milliseconds.
     */
    int64_t getFourthBeat() const {
        return Util::bytesToNumber(packetBytes_.data(), 0x30, 4);
    }

    /**
     * Get the time until the second upcoming bar in milliseconds.
     */
    int64_t getSecondBar() const {
        return Util::bytesToNumber(packetBytes_.data(), 0x34, 4);
    }

    /**
     * Get the time until the eighth beat in milliseconds.
     */
    int64_t getEighthBeat() const {
        return Util::bytesToNumber(packetBytes_.data(), 0x38, 4);
    }

    /**
     * Check if beat-within-bar is meaningful.
     * Players (device number < 33) have meaningful values; mixers don't.
     */
    bool isBeatWithinBarMeaningful() const override {
        // In Java version this checks VirtualCdj, but we simplify here
        return deviceNumber_ < 33;
    }

    /**
     * Check if this device is the tempo master.
     * Note: In Java this requires VirtualCdj to be running.
     */
    bool isTempoMaster() const override {
        // This requires VirtualCdj context in the full implementation
        return false;
    }

    /**
     * Check if this device is synced to the master.
     */
    bool isSynced() const override {
        // This requires VirtualCdj context in the full implementation
        return false;
    }

    /**
     * Beats never yield the master role.
     */
    std::optional<int> getDeviceMasterIsBeingYieldedTo() const override {
        return std::nullopt;
    }

    /**
     * Get the effective tempo (BPM adjusted by pitch).
     */
    double getEffectiveTempo() const override {
        return bpm_ * Util::pitchToMultiplier(pitch_) / 100.0;
    }

    /**
     * Get a string representation.
     */
    std::string toString() const override {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "Beat: Device %d, name: %s, pitch: %+.2f%%, track BPM: %.1f, effective BPM: %.1f, beat within bar: %d",
                 deviceNumber_,
                 deviceName_.c_str(),
                 Util::pitchToPercentage(pitch_),
                 bpm_ / 100.0,
                 getEffectiveTempo(),
                 getBeatWithinBar());
        return std::string(buffer);
    }

private:
    int pitch_;
    int bpm_;
};

} // namespace beatlink
