#pragma once

#include <functional>
#include <memory>

namespace beatlink {

class LifecycleParticipant;

class LifecycleListener {
public:
    virtual ~LifecycleListener() = default;
    virtual void started(LifecycleParticipant& sender) = 0;
    virtual void stopped(LifecycleParticipant& sender) = 0;
};

using LifecycleCallback = std::function<void(LifecycleParticipant&)>;

class LifecycleCallbacks final : public LifecycleListener {
public:
    LifecycleCallbacks(LifecycleCallback started, LifecycleCallback stopped)
        : started_(std::move(started))
        , stopped_(std::move(stopped))
    {
    }

    void started(LifecycleParticipant& sender) override {
        if (started_) {
            started_(sender);
        }
    }

    void stopped(LifecycleParticipant& sender) override {
        if (stopped_) {
            stopped_(sender);
        }
    }

private:
    LifecycleCallback started_;
    LifecycleCallback stopped_;
};

using LifecycleListenerPtr = std::shared_ptr<LifecycleListener>;

} // namespace beatlink
