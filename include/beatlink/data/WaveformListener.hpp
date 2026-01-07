#pragma once

#include <functional>
#include <memory>

#include "WaveformDetailUpdate.hpp"
#include "WaveformPreviewUpdate.hpp"

namespace beatlink::data {

class WaveformListener {
public:
    virtual ~WaveformListener() = default;
    virtual void previewChanged(const WaveformPreviewUpdate& update) = 0;
    virtual void detailChanged(const WaveformDetailUpdate& update) = 0;
};

using WaveformListenerPtr = std::shared_ptr<WaveformListener>;

class WaveformCallbacks final : public WaveformListener {
public:
    using PreviewCallback = std::function<void(const WaveformPreviewUpdate&)>;
    using DetailCallback = std::function<void(const WaveformDetailUpdate&)>;

    WaveformCallbacks(PreviewCallback previewCallback, DetailCallback detailCallback)
        : previewCallback_(std::move(previewCallback))
        , detailCallback_(std::move(detailCallback))
    {
    }

    void previewChanged(const WaveformPreviewUpdate& update) override {
        if (previewCallback_) {
            previewCallback_(update);
        }
    }

    void detailChanged(const WaveformDetailUpdate& update) override {
        if (detailCallback_) {
            detailCallback_(update);
        }
    }

private:
    PreviewCallback previewCallback_;
    DetailCallback detailCallback_;
};

} // namespace beatlink::data
