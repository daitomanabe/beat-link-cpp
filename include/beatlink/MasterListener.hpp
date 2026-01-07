#pragma once

#include <memory>

#include "BeatListener.hpp"

namespace beatlink {

class DeviceUpdate;

class MasterListener : public BeatListener {
public:
    ~MasterListener() override = default;
    virtual void masterChanged(const DeviceUpdate* update) = 0;
    virtual void tempoChanged(double tempo) = 0;
};

using MasterListenerPtr = std::shared_ptr<MasterListener>;

} // namespace beatlink
