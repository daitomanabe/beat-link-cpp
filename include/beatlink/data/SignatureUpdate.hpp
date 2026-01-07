#pragma once

#include <format>
#include <string>

namespace beatlink::data {

class SignatureUpdate {
public:
    SignatureUpdate(int player, std::string signature)
        : player(player)
        , signature(std::move(signature))
    {
    }

    int player;
    std::string signature;

    std::string toString() const {
        return std::format("SignatureUpdate[player:{}, signature:{}]", player, signature);
    }
};

} // namespace beatlink::data
