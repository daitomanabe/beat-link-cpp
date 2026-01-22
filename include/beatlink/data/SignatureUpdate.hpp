#pragma once

#include <fmt/format.h>
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
        return fmt::format("SignatureUpdate[player:{}, signature:{}]", player, signature);
    }
};

} // namespace beatlink::data
