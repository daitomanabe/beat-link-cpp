#pragma once

#include <functional>
#include <memory>

#include "SignatureUpdate.hpp"

namespace beatlink::data {

class SignatureListener {
public:
    virtual ~SignatureListener() = default;
    virtual void signatureChanged(const SignatureUpdate& update) = 0;
};

using SignatureListenerPtr = std::shared_ptr<SignatureListener>;
using SignatureCallback = std::function<void(const SignatureUpdate&)>;

class SignatureCallbacks final : public SignatureListener {
public:
    explicit SignatureCallbacks(SignatureCallback callback)
        : callback_(std::move(callback))
    {
    }

    void signatureChanged(const SignatureUpdate& update) override {
        if (callback_) {
            callback_(update);
        }
    }

private:
    SignatureCallback callback_;
};

} // namespace beatlink::data
