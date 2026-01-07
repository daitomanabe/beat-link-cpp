#pragma once

#include <functional>
#include <memory>

#include "TrackMetadataUpdate.hpp"

namespace beatlink::data {

class TrackMetadataListener {
public:
    virtual ~TrackMetadataListener() = default;
    virtual void metadataChanged(const TrackMetadataUpdate& update) = 0;
};

using TrackMetadataListenerPtr = std::shared_ptr<TrackMetadataListener>;
using TrackMetadataCallback = std::function<void(const TrackMetadataUpdate&)>;

class TrackMetadataCallbacks final : public TrackMetadataListener {
public:
    explicit TrackMetadataCallbacks(TrackMetadataCallback callback)
        : callback_(std::move(callback))
    {
    }

    void metadataChanged(const TrackMetadataUpdate& update) override {
        if (callback_) {
            callback_(update);
        }
    }

private:
    TrackMetadataCallback callback_;
};

} // namespace beatlink::data
