#include "beatlink/Beat.hpp"
#include "beatlink/VirtualCdj.hpp"

#include <fmt/format.h>

namespace beatlink {

bool Beat::isTempoMaster() const {
    auto master = VirtualCdj::getInstance().getTempoMaster();
    if (!master) {
        return false;
    }
    return master->getAddress() == address_ && master->getDeviceNumber() == deviceNumber_;
}

bool Beat::isSynced() const {
    auto status = VirtualCdj::getInstance().getLatestStatusFor(*this);
    if (!status) {
        return false;
    }
    return status->isSynced();
}

std::string Beat::toString() const {
    return fmt::format(
        "Beat: Device {}, name: {}, pitch: {:+.2f}%, track BPM: {:.1f}, effective BPM: {:.1f}, beat within bar: {}",
        deviceNumber_,
        deviceName_,
        Util::pitchToPercentage(pitch_),
        bpm_ / 100.0,
        getEffectiveTempo(),
        getBeatWithinBar());
}

} // namespace beatlink
