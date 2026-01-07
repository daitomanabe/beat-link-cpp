#pragma once

#include <functional>
#include <memory>

#include "SlotReference.hpp"
#include "beatlink/MediaDetailsListener.hpp"

namespace beatlink::data {

class MountListener : public beatlink::MediaDetailsListener {
public:
    virtual ~MountListener() = default;
    virtual void mediaMounted(const SlotReference& slot) = 0;
    virtual void mediaUnmounted(const SlotReference& slot) = 0;

    void detailsAvailable(const beatlink::MediaDetails& /*details*/) override {}
};

using MountListenerPtr = std::shared_ptr<MountListener>;

using MountCallback = std::function<void(const SlotReference&)>;

class MountCallbacks final : public MountListener {
public:
    MountCallbacks(MountCallback mounted, MountCallback unmounted)
        : mounted_(std::move(mounted))
        , unmounted_(std::move(unmounted))
    {
    }

    void mediaMounted(const SlotReference& slot) override {
        if (mounted_) {
            mounted_(slot);
        }
    }

    void mediaUnmounted(const SlotReference& slot) override {
        if (unmounted_) {
            unmounted_(slot);
        }
    }

private:
    MountCallback mounted_;
    MountCallback unmounted_;
};

} // namespace beatlink::data
