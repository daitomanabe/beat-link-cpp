#pragma once

#include <functional>
#include <memory>

namespace beatlink {

class Beat;

class BeatListener {
public:
    virtual ~BeatListener() = default;
    virtual void newBeat(const Beat& beat) = 0;
};

using BeatListenerPtr = std::shared_ptr<BeatListener>;
using BeatCallback = std::function<void(const Beat&)>;

class BeatListenerCallbacks final : public BeatListener {
public:
    explicit BeatListenerCallbacks(BeatCallback callback)
        : callback_(std::move(callback))
    {
    }

    void newBeat(const Beat& beat) override {
        if (callback_) {
            callback_(beat);
        }
    }

private:
    BeatCallback callback_;
};

} // namespace beatlink
