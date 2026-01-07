#pragma once

#include <functional>
#include <memory>

#include "TrackPositionUpdate.hpp"

namespace beatlink::data {

class TrackPositionListener {
public:
    virtual ~TrackPositionListener() = default;
    virtual void movementChanged(const std::shared_ptr<TrackPositionUpdate>& update) = 0;
};

using TrackPositionListenerPtr = std::shared_ptr<TrackPositionListener>;
using TrackPositionCallback = std::function<void(const std::shared_ptr<TrackPositionUpdate>&)>;

class TrackPositionCallbacks final : public TrackPositionListener {
public:
    explicit TrackPositionCallbacks(TrackPositionCallback callback)
        : callback_(std::move(callback))
    {
    }

    void movementChanged(const std::shared_ptr<TrackPositionUpdate>& update) override {
        if (callback_) {
            callback_(update);
        }
    }

private:
    TrackPositionCallback callback_;
};

} // namespace beatlink::data
