#pragma once

#include <functional>
#include <memory>

#include "AnalysisTagUpdate.hpp"

namespace beatlink::data {

class AnalysisTagListener {
public:
    virtual ~AnalysisTagListener() = default;
    virtual void analysisChanged(const AnalysisTagUpdate& update) = 0;
};

using AnalysisTagListenerPtr = std::shared_ptr<AnalysisTagListener>;
using AnalysisTagCallback = std::function<void(const AnalysisTagUpdate&)>;

class AnalysisTagCallbacks final : public AnalysisTagListener {
public:
    explicit AnalysisTagCallbacks(AnalysisTagCallback callback)
        : callback_(std::move(callback))
    {
    }

    void analysisChanged(const AnalysisTagUpdate& update) override {
        if (callback_) {
            callback_(update);
        }
    }

private:
    AnalysisTagCallback callback_;
};

} // namespace beatlink::data
