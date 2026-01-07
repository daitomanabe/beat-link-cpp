#pragma once

#include <functional>
#include <memory>

#include "BeatGridUpdate.hpp"

namespace beatlink::data {

class BeatGridListener {
public:
    virtual ~BeatGridListener() = default;
    virtual void beatGridChanged(const BeatGridUpdate& update) = 0;
};

using BeatGridListenerPtr = std::shared_ptr<BeatGridListener>;
using BeatGridCallback = std::function<void(const BeatGridUpdate&)>;

class BeatGridCallbacks final : public BeatGridListener {
public:
    explicit BeatGridCallbacks(BeatGridCallback callback)
        : callback_(std::move(callback))
    {
    }

    void beatGridChanged(const BeatGridUpdate& update) override {
        if (callback_) {
            callback_(update);
        }
    }

private:
    BeatGridCallback callback_;
};

} // namespace beatlink::data
