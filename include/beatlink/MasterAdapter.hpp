#pragma once

#include "MasterListener.hpp"

namespace beatlink {

class MasterAdapter : public MasterListener {
public:
    void masterChanged(const DeviceUpdate* /*update*/) override {}
    void newBeat(const Beat& /*beat*/) override {}
    void tempoChanged(double /*tempo*/) override {}
};

} // namespace beatlink
