#pragma once

#include "DeviceUpdate.hpp"
#include "Util.hpp"

namespace beatlink {

class PrecisePosition : public DeviceUpdate {
public:
    static constexpr size_t PACKET_SIZE = 0x3c; // 60 bytes

    PrecisePosition(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress)
        : DeviceUpdate(data, length, senderAddress, "Precise position", PACKET_SIZE, DEVICE_NUMBER_OFFSET)
    {
        trackLength_ = static_cast<int>(Util::bytesToNumber(data, 0x24, 4));
        playbackPosition_ = static_cast<int>(Util::bytesToNumber(data, 0x28, 4));
        int64_t rawPitch = Util::bytesToNumber(data, 0x2c, 4);
        if (rawPitch > 0x80000000LL) {
            rawPitch -= 0x100000000LL;
        }
        pitch_ = static_cast<int>(Util::percentageToPitch(rawPitch / 100.0));
        bpm_ = static_cast<int>(Util::bytesToNumber(data, 0x38, 4)) * 10;
    }

    int getTrackLength() const { return trackLength_; }
    int getPlaybackPosition() const { return playbackPosition_; }

    int getPitch() const override { return pitch_; }

    int getBpm() const override;
    bool isTempoMaster() const override;
    bool isSynced() const override;
    std::optional<int> getDeviceMasterIsBeingYieldedTo() const override { return std::nullopt; }
    double getEffectiveTempo() const override { return bpm_ / 100.0; }
    int getBeatWithinBar() const override;
    bool isBeatWithinBarMeaningful() const override;

    std::string toString() const override;

private:
    int trackLength_{0};
    int playbackPosition_{0};
    int pitch_{0};
    int bpm_{0};
};

} // namespace beatlink
