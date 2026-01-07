#pragma once

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "LifecycleListener.hpp"

namespace beatlink {

class LifecycleParticipant {
public:
    virtual ~LifecycleParticipant() = default;

    void addLifecycleListener(const LifecycleListenerPtr& listener) {
        if (!listener) {
            return;
        }
        std::lock_guard<std::mutex> lock(lifecycleMutex_);
        lifecycleListeners_.push_back(listener);
    }

    void addLifecycleListener(LifecycleCallback started, LifecycleCallback stopped) {
        addLifecycleListener(std::make_shared<LifecycleCallbacks>(std::move(started), std::move(stopped)));
    }

    void removeLifecycleListener(const LifecycleListenerPtr& listener) {
        if (!listener) {
            return;
        }
        std::lock_guard<std::mutex> lock(lifecycleMutex_);
        lifecycleListeners_.erase(
            std::remove(lifecycleListeners_.begin(), lifecycleListeners_.end(), listener),
            lifecycleListeners_.end());
    }

    std::vector<LifecycleListenerPtr> getLifecycleListeners() const {
        std::lock_guard<std::mutex> lock(lifecycleMutex_);
        return lifecycleListeners_;
    }

    virtual bool isRunning() const = 0;

protected:
    void deliverLifecycleAnnouncement(bool starting) {
        auto listeners = getLifecycleListeners();
        std::thread([this, listeners, starting]() mutable {
            for (const auto& listener : listeners) {
                if (!listener) {
                    continue;
                }
                try {
                    if (starting) {
                        listener->started(*this);
                    } else {
                        listener->stopped(*this);
                    }
                } catch (...) {
                    // Swallow listener errors to avoid destabilizing the lifecycle thread.
                }
            }
        }).detach();
    }

    void ensureRunning() const {
        if (!isRunning()) {
            throw std::runtime_error("LifecycleParticipant is not running");
        }
    }

private:
    mutable std::mutex lifecycleMutex_;
    std::vector<LifecycleListenerPtr> lifecycleListeners_;
};

} // namespace beatlink
