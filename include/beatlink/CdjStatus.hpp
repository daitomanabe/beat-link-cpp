#pragma once

#include "DeviceUpdate.hpp"
#include "Util.hpp"

namespace beatlink {

/**
 * Track source slot types.
 */
enum class TrackSourceSlot : uint8_t {
    NO_TRACK = 0,
    CD_SLOT = 1,
    SD_SLOT = 2,
    USB_SLOT = 3,
    COLLECTION = 4,
    USB_2_SLOT = 7,
    UNKNOWN = 0xff
};

/**
 * Track type values.
 */
enum class TrackType : uint8_t {
    NO_TRACK = 0,
    REKORDBOX = 1,
    UNANALYZED = 2,
    CD_DIGITAL_AUDIO = 5,
    UNKNOWN = 0xff
};

/**
 * Play state values.
 */
enum class PlayState : uint8_t {
    NO_TRACK = 0,
    LOADING = 2,
    PLAYING = 3,
    LOOPING = 4,
    PAUSED = 5,
    CUED = 6,
    CUEING = 7,
    SEARCHING = 9,
    SPUN_DOWN = 0x0e,
    ENDED = 0x11,
    UNKNOWN = 0xff
};

/**
 * Represents a status update sent by a CDJ on a DJ Link network.
 * Ported from org.deepsymmetry.beatlink.CdjStatus
 */
class CdjStatus : public DeviceUpdate {
public:
    // Status flag bits
    static constexpr int STATUS_FLAGS = 0x89;
    static constexpr int MASTER_HAND_OFF = 0x9f;
    static constexpr int BPM_SYNC_FLAG = 0x02;
    static constexpr int ON_AIR_FLAG = 0x08;
    static constexpr int SYNCED_FLAG = 0x10;
    static constexpr int MASTER_FLAG = 0x20;
    static constexpr int PLAYING_FLAG = 0x40;

    // Slot state offsets
    static constexpr int LOCAL_CD_STATE = 0x37;
    static constexpr int LOCAL_USB_STATE = 0x6F;
    static constexpr int LOCAL_SD_STATE = 0x73;

    /**
     * Minimum packet size for CDJ status.
     */
    static constexpr size_t MIN_PACKET_SIZE = 0xcc; // 204 bytes minimum

    /**
     * Construct from raw packet data.
     */
    CdjStatus(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress)
        : DeviceUpdate(data, length, senderAddress, "CDJ status", length)
    {
        // Parse track source player (offset 0x28)
        trackSourcePlayer_ = data[0x28];

        // Parse track source slot (offset 0x29)
        uint8_t slotByte = data[0x29];
        switch (slotByte) {
            case 0: trackSourceSlot_ = TrackSourceSlot::NO_TRACK; break;
            case 1: trackSourceSlot_ = TrackSourceSlot::CD_SLOT; break;
            case 2: trackSourceSlot_ = TrackSourceSlot::SD_SLOT; break;
            case 3: trackSourceSlot_ = TrackSourceSlot::USB_SLOT; break;
            case 4: trackSourceSlot_ = TrackSourceSlot::COLLECTION; break;
            case 7: trackSourceSlot_ = TrackSourceSlot::USB_2_SLOT; break;
            default: trackSourceSlot_ = TrackSourceSlot::UNKNOWN; break;
        }

        // Parse track type (offset 0x2a)
        uint8_t typeByte = data[0x2a];
        switch (typeByte) {
            case 0: trackType_ = TrackType::NO_TRACK; break;
            case 1: trackType_ = TrackType::REKORDBOX; break;
            case 2: trackType_ = TrackType::UNANALYZED; break;
            case 5: trackType_ = TrackType::CD_DIGITAL_AUDIO; break;
            default: trackType_ = TrackType::UNKNOWN; break;
        }

        // Parse rekordbox ID (offset 0x2c, 4 bytes)
        rekordboxId_ = static_cast<int>(Util::bytesToNumber(data, 0x2c, 4));

        // Parse track number (offset 0x32, 2 bytes)
        trackNumber_ = static_cast<int>(Util::bytesToNumber(data, 0x32, 2));

        // Parse BPM (offset 0x92, 2 bytes)
        bpm_ = static_cast<int>(Util::bytesToNumber(data, 0x92, 2));

        // Parse pitch (offset 0x8d, 4 bytes, but only 3 bytes meaningful)
        pitch_ = static_cast<int>(Util::bytesToNumber(data, 0x8d, 4));

        // Parse beat within bar (offset 0xa6)
        beatWithinBar_ = data[0xa6];

        // Parse packet counter (offset 0xc8, 4 bytes)
        packetCounter_ = static_cast<int>(Util::bytesToNumber(data, 0xc8, 4));
    }

    // Track source information
    int getTrackSourcePlayer() const { return trackSourcePlayer_; }
    TrackSourceSlot getTrackSourceSlot() const { return trackSourceSlot_; }
    TrackType getTrackType() const { return trackType_; }
    int getRekordboxId() const { return rekordboxId_; }
    int getTrackNumber() const { return trackNumber_; }
    int getPacketCounter() const { return packetCounter_; }

    // Status flags
    uint8_t getStatusFlags() const {
        return packetBytes_.size() > STATUS_FLAGS ? packetBytes_[STATUS_FLAGS] : 0;
    }

    bool isPlaying() const {
        return (getStatusFlags() & PLAYING_FLAG) != 0;
    }

    bool isTempoMaster() const override {
        return (getStatusFlags() & MASTER_FLAG) != 0;
    }

    bool isSynced() const override {
        return (getStatusFlags() & SYNCED_FLAG) != 0;
    }

    bool isOnAir() const {
        return (getStatusFlags() & ON_AIR_FLAG) != 0;
    }

    bool isBpmOnlySynced() const {
        return (getStatusFlags() & BPM_SYNC_FLAG) != 0;
    }

    std::optional<int> getDeviceMasterIsBeingYieldedTo() const override {
        if (packetBytes_.size() <= MASTER_HAND_OFF) {
            return std::nullopt;
        }
        uint8_t value = packetBytes_[MASTER_HAND_OFF];
        if (value != 0xff) {
            return static_cast<int>(value);
        }
        return std::nullopt;
    }

    // Tempo information
    int getPitch() const override { return pitch_; }
    int getBpm() const override { return bpm_; }

    double getEffectiveTempo() const override {
        return bpm_ * Util::pitchToMultiplier(pitch_) / 100.0;
    }

    int getBeatWithinBar() const override { return beatWithinBar_; }

    bool isBeatWithinBarMeaningful() const override {
        return true; // CDJs always have meaningful beat-within-bar
    }

    /**
     * Check if a track is loaded.
     */
    bool isTrackLoaded() const {
        return trackType_ != TrackType::NO_TRACK;
    }

    /**
     * Check if paused at cue point.
     */
    bool isAtCue() const {
        return getPlayState() == PlayState::CUED;
    }

    /**
     * Get the current play state.
     */
    PlayState getPlayState() const {
        if (packetBytes_.size() <= 0x7b) {
            return PlayState::UNKNOWN;
        }
        uint8_t state = packetBytes_[0x7b];
        switch (state) {
            case 0x00: return PlayState::NO_TRACK;
            case 0x02: return PlayState::LOADING;
            case 0x03: return PlayState::PLAYING;
            case 0x04: return PlayState::LOOPING;
            case 0x05: return PlayState::PAUSED;
            case 0x06: return PlayState::CUED;
            case 0x07: return PlayState::CUEING;
            case 0x09: return PlayState::SEARCHING;
            case 0x0e: return PlayState::SPUN_DOWN;
            case 0x11: return PlayState::ENDED;
            default: return PlayState::UNKNOWN;
        }
    }

    std::string toString() const override {
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                 "CdjStatus: Device %d, name: %s, playing: %s, master: %s, synced: %s, "
                 "BPM: %.2f, effective BPM: %.2f, pitch: %+.2f%%, beat: %d",
                 deviceNumber_,
                 deviceName_.c_str(),
                 isPlaying() ? "true" : "false",
                 isTempoMaster() ? "true" : "false",
                 isSynced() ? "true" : "false",
                 bpm_ / 100.0,
                 getEffectiveTempo(),
                 Util::pitchToPercentage(pitch_),
                 beatWithinBar_);
        return std::string(buffer);
    }

private:
    int trackSourcePlayer_;
    TrackSourceSlot trackSourceSlot_;
    TrackType trackType_;
    int rekordboxId_;
    int trackNumber_;
    int bpm_;
    int pitch_;
    int beatWithinBar_;
    int packetCounter_;
};

} // namespace beatlink
