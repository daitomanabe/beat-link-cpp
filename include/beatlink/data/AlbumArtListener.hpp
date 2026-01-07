#pragma once

#include <functional>
#include <memory>

#include "AlbumArtUpdate.hpp"

namespace beatlink::data {

class AlbumArtListener {
public:
    virtual ~AlbumArtListener() = default;
    virtual void albumArtChanged(const AlbumArtUpdate& update) = 0;
};

using AlbumArtListenerPtr = std::shared_ptr<AlbumArtListener>;
using AlbumArtCallback = std::function<void(const AlbumArtUpdate&)>;

class AlbumArtCallbacks final : public AlbumArtListener {
public:
    explicit AlbumArtCallbacks(AlbumArtCallback callback)
        : callback_(std::move(callback))
    {
    }

    void albumArtChanged(const AlbumArtUpdate& update) override {
        if (callback_) {
            callback_(update);
        }
    }

private:
    AlbumArtCallback callback_;
};

} // namespace beatlink::data
