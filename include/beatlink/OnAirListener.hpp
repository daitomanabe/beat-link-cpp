#pragma once

#include <set>

namespace beatlink {

class OnAirListener {
public:
    virtual ~OnAirListener() = default;
    virtual void channelsOnAir(const std::set<int>& audibleChannels) = 0;
};

} // namespace beatlink
