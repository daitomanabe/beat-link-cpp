#pragma once

#include <functional>
#include <memory>

namespace beatlink {

class MediaDetails;

class MediaDetailsListener {
public:
    virtual ~MediaDetailsListener() = default;
    virtual void detailsAvailable(const MediaDetails& details) = 0;
};

using MediaDetailsListenerPtr = std::shared_ptr<MediaDetailsListener>;
using MediaDetailsCallback = std::function<void(const MediaDetails&)>;

class MediaDetailsAdapter : public MediaDetailsListener {
public:
    void detailsAvailable(const MediaDetails& /*details*/) override {}
};

class MediaDetailsCallbacks final : public MediaDetailsListener {
public:
    explicit MediaDetailsCallbacks(MediaDetailsCallback callback)
        : callback_(std::move(callback))
    {
    }

    void detailsAvailable(const MediaDetails& details) override {
        if (callback_) {
            callback_(details);
        }
    }

private:
    MediaDetailsCallback callback_;
};

} // namespace beatlink
