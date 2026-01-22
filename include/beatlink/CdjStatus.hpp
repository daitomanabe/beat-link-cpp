#pragma once

#include "DeviceUpdate.hpp"
#include "Util.hpp"

#include <fmt/format.h>
#include <optional>
#include "Span.hpp"

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
 * Secondary play state values.
 */
enum class PlayState2 : uint8_t {
    MOVING = 0x7a,
    STOPPED = 0x7e,
    OPUS_MOVING = 0xfa,
    UNKNOWN = 0xff
};

/**
 * Tertiary play state values.
 */
enum class PlayState3 : uint8_t {
    NO_TRACK = 0x00,
    PAUSED_OR_REVERSE = 0x01,
    FORWARD_VINYL = 0x09,
    FORWARD_CDJ = 0x0d,
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
     * Factory function to create a CdjStatus from raw packet data.
     * Returns std::nullopt if packet size is too small.
     *
     * @param data the packet bytes as span
     * @param senderAddress the IP address of the sender
     * @return std::optional<CdjStatus> - nullopt if invalid
     */
    [[nodiscard]] static std::optional<CdjStatus> create(
        std::span<const uint8_t> data,
        const asio::ip::address_v4& senderAddress
    ) noexcept {
        if (!validatePacketSize(data.size(), MIN_PACKET_SIZE)) {
            return std::nullopt;
        }
        return CdjStatus(data, senderAddress, NoValidateTag{});
    }

    /**
     * Construct from raw packet data.
     * @deprecated Use create() factory function for exception-safe construction
     */
    CdjStatus(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress)
        : DeviceUpdate(data, length, senderAddress, "CDJ status", length)
    {
        initParsing(data);
    }

    // Track source information
    int getTrackSourcePlayer() const { return trackSourcePlayer_; }
    TrackSourceSlot getTrackSourceSlot() const { return trackSourceSlot_; }
    TrackType getTrackType() const { return trackType_; }
    int getRekordboxId() const { return rekordboxId_; }
    int getTrackNumber() const { return trackNumber_; }
    int getPacketCounter() const { return packetCounter_; }
    int getSyncNumber() const { return syncNumber_; }
    int getBeatNumber() const { return beatNumber_; }

    // Status flags
    uint8_t getStatusFlags() const {
        return packetBytes_.size() > STATUS_FLAGS ? packetBytes_[STATUS_FLAGS] : 0;
    }

    bool isPlaying() const {
        if (packetBytes_.size() >= 0xd4) {
            return (getStatusFlags() & PLAYING_FLAG) != 0;
        }
        return playState1_ == PlayState::PLAYING || playState1_ == PlayState::LOOPING ||
               (playState1_ == PlayState::SEARCHING && playState2_ == PlayState2::MOVING);
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

    bool isLocalUsbLoaded() const {
        return packetBytes_.size() > LOCAL_USB_STATE && packetBytes_[LOCAL_USB_STATE] == 0;
    }

    bool isLocalUsbUnloading() const {
        return packetBytes_.size() > LOCAL_USB_STATE && packetBytes_[LOCAL_USB_STATE] == 2;
    }

    bool isLocalUsbEmpty() const {
        return packetBytes_.size() > LOCAL_USB_STATE && packetBytes_[LOCAL_USB_STATE] == 4;
    }

    bool isLocalSdLoaded() const {
        return packetBytes_.size() > LOCAL_SD_STATE && packetBytes_[LOCAL_SD_STATE] == 0;
    }

    bool isLocalSdUnloading() const {
        return packetBytes_.size() > LOCAL_SD_STATE && packetBytes_[LOCAL_SD_STATE] == 2;
    }

    bool isLocalSdEmpty() const {
        return packetBytes_.size() > LOCAL_SD_STATE && packetBytes_[LOCAL_SD_STATE] == 4;
    }

    bool isDiscSlotEmpty() const {
        if (packetBytes_.size() <= LOCAL_CD_STATE) {
            return true;
        }
        return (packetBytes_[LOCAL_CD_STATE] != 0x1e) && (packetBytes_[LOCAL_CD_STATE] != 0x11);
    }

    bool isDiscSlotAsleep() const {
        return packetBytes_.size() > LOCAL_CD_STATE && packetBytes_[LOCAL_CD_STATE] == 1;
    }

    int getDiscTrackCount() const {
        if (packetBytes_.size() <= 0x48) {
            return 0;
        }
        return static_cast<int>(Util::bytesToNumber(packetBytes_.data(), 0x46, 2));
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
        return playState1_;
    }

    PlayState2 getPlayState2() const {
        return playState2_;
    }

    PlayState3 getPlayState3() const {
        return playState3_;
    }

    bool isPlayingForwards() const {
        return playState1_ == PlayState::PLAYING && playState3_ != PlayState3::PAUSED_OR_REVERSE;
    }

    bool isPlayingBackwards() const {
        return playState1_ == PlayState::PLAYING && playState3_ == PlayState3::PAUSED_OR_REVERSE;
    }

    std::string toString() const override {
        return fmt::format(
            "CdjStatus: Device {}, name: {}, playing: {}, master: {}, synced: {}, BPM: {:.2f}, effective BPM: {:.2f}, pitch: {:+.2f}%, beat: {}",
            deviceNumber_,
            deviceName_,
            isPlaying() ? "true" : "false",
            isTempoMaster() ? "true" : "false",
            isSynced() ? "true" : "false",
            bpm_ / 100.0,
            getEffectiveTempo(),
            Util::pitchToPercentage(pitch_),
            beatWithinBar_);
    }

private:
    /**
     * Private constructor for factory function (noexcept).
     */
    CdjStatus(std::span<const uint8_t> data, const asio::ip::address_v4& senderAddress, NoValidateTag) noexcept
        : DeviceUpdate(data, senderAddress, DEVICE_NUMBER_OFFSET, NoValidateTag{})
    {
        initParsing(data.data());
    }

    void initParsing(const uint8_t* data) noexcept {
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

        // Parse play states
        playState1_ = parsePlayState1();
        playState2_ = parsePlayState2();
        playState3_ = parsePlayState3();

        // Parse sync counter (offset 0x84, 4 bytes)
        syncNumber_ = static_cast<int>(Util::bytesToNumber(data, 0x84, 4));

        // Parse beat number (offset 0xa0, 4 bytes)
        int64_t beatNumber = Util::bytesToNumber(data, 0xa0, 4);
        beatNumber_ = (beatNumber == 0xffffffff) ? -1 : static_cast<int>(beatNumber);

        // Parse packet counter (offset 0xc8, 4 bytes)
        packetCounter_ = static_cast<int>(Util::bytesToNumber(data, 0xc8, 4));
    }

    PlayState parsePlayState1() const {
        if (packetBytes_.size() <= 0x7b) {
            return PlayState::UNKNOWN;
        }
        switch (packetBytes_[0x7b]) {
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

    PlayState2 parsePlayState2() const {
        if (packetBytes_.size() <= 0x8b) {
            return PlayState2::UNKNOWN;
        }
        switch (packetBytes_[0x8b]) {
            case 0x6a:
            case 0x7a:
            case static_cast<uint8_t>(0xfa):
                return PlayState2::MOVING;
            case 0x6e:
            case 0x7e:
            case static_cast<uint8_t>(0xfe):
                return PlayState2::STOPPED;
            default:
                return PlayState2::UNKNOWN;
        }
    }

    PlayState3 parsePlayState3() const {
        if (packetBytes_.size() <= 0x9d) {
            return PlayState3::UNKNOWN;
        }
        switch (packetBytes_[0x9d]) {
            case 0x00:
                return PlayState3::NO_TRACK;
            case 0x01:
                return PlayState3::PAUSED_OR_REVERSE;
            case 0x09:
                return PlayState3::FORWARD_VINYL;
            case 0x0d:
                return PlayState3::FORWARD_CDJ;
            default:
                return PlayState3::UNKNOWN;
        }
    }

    int trackSourcePlayer_;
    TrackSourceSlot trackSourceSlot_;
    TrackType trackType_;
    int rekordboxId_;
    int trackNumber_;
    int bpm_;
    int pitch_;
    int beatWithinBar_;
    int packetCounter_;
    int syncNumber_;
    int beatNumber_;
    PlayState playState1_{PlayState::UNKNOWN};
    PlayState2 playState2_{PlayState2::UNKNOWN};
    PlayState3 playState3_{PlayState3::UNKNOWN};
};

} // namespace beatlink
