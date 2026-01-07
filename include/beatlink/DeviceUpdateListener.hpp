#pragma once

#include <functional>
#include <memory>

namespace beatlink {

class DeviceUpdate;

class DeviceUpdateListener {
public:
    virtual ~DeviceUpdateListener() = default;
    virtual void received(const DeviceUpdate& update) = 0;
};

using DeviceUpdateListenerPtr = std::shared_ptr<DeviceUpdateListener>;
using DeviceUpdateCallback = std::function<void(const DeviceUpdate&)>;

class DeviceUpdateCallbacks final : public DeviceUpdateListener {
public:
    explicit DeviceUpdateCallbacks(DeviceUpdateCallback callback)
        : callback_(std::move(callback))
    {
    }

    void received(const DeviceUpdate& update) override {
        if (callback_) {
            callback_(update);
        }
    }

private:
    DeviceUpdateCallback callback_;
};

} // namespace beatlink
