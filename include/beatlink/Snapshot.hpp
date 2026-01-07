#pragma once

#include <cstdint>

namespace beatlink {

struct Snapshot {
    int64_t instant_ms = 0;
    int64_t start_ms = 0;
    double tempo = 120.0;
    int beats_per_bar = 4;
    int64_t beat = 1;
    double beat_phase = 0.0;

    [[nodiscard]] double getBeatInterval() const { return 60000.0 / tempo; }
    [[nodiscard]] double getBarInterval() const { return getBeatInterval() * beats_per_bar; }
    [[nodiscard]] int64_t getBeat() const { return beat; }
    [[nodiscard]] int getBeatWithinBar() const {
        int result = static_cast<int>((beat - 1) % beats_per_bar) + 1;
        return result == 0 ? 1 : result;
    }
    [[nodiscard]] double getBeatPhase() const { return beat_phase; }
    [[nodiscard]] int64_t getInstant() const { return instant_ms; }
    [[nodiscard]] double getTempo() const { return tempo; }

    [[nodiscard]] int64_t getTimeOfBeat(int64_t targetBeat) const {
        return start_ms + static_cast<int64_t>((targetBeat - 1) * getBeatInterval());
    }

    [[nodiscard]] double distanceFromBeat() const {
        return static_cast<double>(instant_ms - getTimeOfBeat(beat));
    }
};

} // namespace beatlink
