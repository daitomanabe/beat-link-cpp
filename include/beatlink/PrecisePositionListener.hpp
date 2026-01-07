#pragma once

#include <functional>
#include <memory>

namespace beatlink {

class PrecisePosition;

class PrecisePositionListener {
public:
    virtual ~PrecisePositionListener() = default;
    virtual void positionReported(const PrecisePosition& position) = 0;
};

using PrecisePositionListenerPtr = std::shared_ptr<PrecisePositionListener>;
using PrecisePositionCallback = std::function<void(const PrecisePosition&)>;

class PrecisePositionCallbacks final : public PrecisePositionListener {
public:
    explicit PrecisePositionCallbacks(PrecisePositionCallback callback)
        : callback_(std::move(callback))
    {
    }

    void positionReported(const PrecisePosition& position) override {
        if (callback_) {
            callback_(position);
        }
    }

private:
    PrecisePositionCallback callback_;
};

} // namespace beatlink
