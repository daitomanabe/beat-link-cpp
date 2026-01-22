#include "beatlink/PrecisePosition.hpp"
#include "beatlink/VirtualCdj.hpp"

#include <cmath>
#include <fmt/format.h>

namespace beatlink {

int PrecisePosition::getBpm() const {
    double multiplier = Util::pitchToMultiplier(pitch_);
    if (multiplier <= 0.0) {
        return 0;
    }
    return static_cast<int>(std::round(bpm_ / multiplier));
}

bool PrecisePosition::isTempoMaster() const {
    auto master = VirtualCdj::getInstance().getTempoMaster();
    if (!master) {
        return false;
    }
    return master->getAddress() == address_ && master->getDeviceNumber() == deviceNumber_;
}

bool PrecisePosition::isSynced() const {
    auto status = VirtualCdj::getInstance().getLatestStatusFor(*this);
    if (!status) {
        return false;
    }
    return status->isSynced();
}

int PrecisePosition::getBeatWithinBar() const {
    auto status = VirtualCdj::getInstance().getLatestStatusFor(*this);
    if (!status) {
        return 0;
    }
    return status->getBeatWithinBar();
}

bool PrecisePosition::isBeatWithinBarMeaningful() const {
    auto status = VirtualCdj::getInstance().getLatestStatusFor(*this);
    if (!status) {
        return false;
    }
    return status->isBeatWithinBarMeaningful();
}

std::string PrecisePosition::toString() const {
    return fmt::format(
        "Precise position: Device {}, name: {}, pitch: {:+.2f}%, track BPM: {:.1f}, effective BPM: {:.1f}",
        deviceNumber_,
        deviceName_,
        Util::pitchToPercentage(pitch_),
        getBpm() / 100.0,
        getEffectiveTempo());
}

} // namespace beatlink
