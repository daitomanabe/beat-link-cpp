#pragma once

#include <functional>
#include <memory>

#include "../Beat.hpp"
#include "TrackPositionListener.hpp"

namespace beatlink::data {

class TrackPositionBeatListener : public TrackPositionListener {
public:
    virtual ~TrackPositionBeatListener() = default;
    virtual void newBeat(const beatlink::Beat& beat,
                         const std::shared_ptr<TrackPositionUpdate>& update) = 0;
};

using TrackPositionBeatListenerPtr = std::shared_ptr<TrackPositionBeatListener>;
using TrackPositionBeatCallback = std::function<void(const beatlink::Beat&,
                                                     const std::shared_ptr<TrackPositionUpdate>&)>;

class TrackPositionBeatCallbacks final : public TrackPositionBeatListener {
public:
    TrackPositionBeatCallbacks(TrackPositionCallback movementCallback,
                               TrackPositionBeatCallback beatCallback)
        : movementCallback_(std::move(movementCallback))
        , beatCallback_(std::move(beatCallback))
    {
    }

    void movementChanged(const std::shared_ptr<TrackPositionUpdate>& update) override {
        if (movementCallback_) {
            movementCallback_(update);
        }
    }

    void newBeat(const beatlink::Beat& beat,
                 const std::shared_ptr<TrackPositionUpdate>& update) override {
        if (beatCallback_) {
            beatCallback_(beat, update);
        }
    }

private:
    TrackPositionCallback movementCallback_;
    TrackPositionBeatCallback beatCallback_;
};

} // namespace beatlink::data
