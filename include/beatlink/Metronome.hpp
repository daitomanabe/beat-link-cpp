#pragma once

#include <atomic>
#include <chrono>

#include "Snapshot.hpp"

namespace beatlink {

class Metronome {
public:
    Metronome();

    [[nodiscard]] Snapshot getSnapshot() const;
    [[nodiscard]] Snapshot getSnapshot(int64_t time_ms) const;

    [[nodiscard]] int64_t getStartTime() const { return start_time_ms_.load(); }
    [[nodiscard]] double getTempo() const { return tempo_.load(); }
    [[nodiscard]] int getBeatsPerBar() const { return beats_per_bar_; }

    void setTempo(double tempo);
    void setBeatPhase(double phase);
    void jumpToBeat(int64_t beat);
    void adjustStart(int64_t delta_ms);

    [[nodiscard]] int64_t getBeat() const { return getSnapshot().beat; }
    [[nodiscard]] int64_t getTimeOfBeat(int64_t beat) const;

    static double beatsToMilliseconds(int beats, double tempo);

private:
    static int64_t nowMs();

    std::atomic<int64_t> start_time_ms_;
    std::atomic<double> tempo_;
    int beats_per_bar_ = 4;
};

} // namespace beatlink
