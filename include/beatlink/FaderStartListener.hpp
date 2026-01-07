#pragma once

#include <set>

namespace beatlink {

class FaderStartListener {
public:
    virtual ~FaderStartListener() = default;
    virtual void fadersChanged(const std::set<int>& playersToStart,
                               const std::set<int>& playersToStop) = 0;
};

} // namespace beatlink
