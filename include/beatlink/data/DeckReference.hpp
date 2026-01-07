#pragma once

#include <format>
#include <functional>

namespace beatlink::data {

class DeckReference {
public:
    DeckReference(int player, int hotCue)
        : player_(player)
        , hotCue_(hotCue)
    {
        std::size_t scratch = 7;
        scratch = scratch * 31 + static_cast<std::size_t>(player_);
        hash_ = scratch * 31 + static_cast<std::size_t>(hotCue_);
    }

    int getPlayer() const { return player_; }
    int getHotCue() const { return hotCue_; }

    std::string toString() const {
        return std::format("DeckReference[player:{}, hotCue:{}]", player_, hotCue_);
    }

    bool operator==(const DeckReference& other) const {
        return player_ == other.player_ && hotCue_ == other.hotCue_;
    }

    bool operator!=(const DeckReference& other) const {
        return !(*this == other);
    }

    std::size_t hash() const { return hash_; }

private:
    int player_;
    int hotCue_;
    std::size_t hash_;
};

struct DeckReferenceHash {
    std::size_t operator()(const DeckReference& ref) const {
        return ref.hash();
    }
};

} // namespace beatlink::data
