#include "beatlink/Metronome.hpp"

#include <cmath>

namespace beatlink {

Metronome::Metronome()
    : start_time_ms_(nowMs())
    , tempo_(120.0)
{
}

Snapshot Metronome::getSnapshot() const {
    return getSnapshot(nowMs());
}

Snapshot Metronome::getSnapshot(int64_t time_ms) const {
    Snapshot snapshot;
    snapshot.instant_ms = time_ms;
    snapshot.start_ms = start_time_ms_.load();
    snapshot.tempo = tempo_.load();
    snapshot.beats_per_bar = beats_per_bar_;

    const double beat_interval = 60000.0 / snapshot.tempo;
    const double beat_pos = (time_ms - snapshot.start_ms) / beat_interval + 1.0;
    const auto beat_int = static_cast<int64_t>(std::floor(beat_pos));

    snapshot.beat = beat_int < 1 ? 1 : beat_int;
    snapshot.beat_phase = beat_pos - static_cast<double>(snapshot.beat);
    if (snapshot.beat_phase < 0.0) {
        snapshot.beat_phase = 0.0;
    }

    return snapshot;
}

void Metronome::setTempo(double tempo) {
    if (tempo <= 0.0) {
        return;
    }

    const int64_t now = nowMs();
    const Snapshot snapshot = getSnapshot(now);

    tempo_.store(tempo);
    const double beat_interval = 60000.0 / tempo;
    const int64_t new_start = now - static_cast<int64_t>((snapshot.beat - 1) * beat_interval);
    start_time_ms_.store(new_start);
}

void Metronome::setBeatPhase(double phase) {
    if (phase < 0.0) {
        phase = 0.0;
    }
    if (phase > 1.0) {
        phase = 1.0;
    }
    const int64_t now = nowMs();
    const Snapshot snapshot = getSnapshot(now);
    const double beat_interval = 60000.0 / tempo_.load();
    const double adjustedBeat = static_cast<double>(snapshot.beat - 1) + phase;
    const int64_t new_start = now - static_cast<int64_t>(adjustedBeat * beat_interval);
    start_time_ms_.store(new_start);
}

void Metronome::jumpToBeat(int64_t beat) {
    if (beat < 1) {
        beat = 1;
    }
    const int64_t now = nowMs();
    const double beat_interval = 60000.0 / tempo_.load();
    const int64_t new_start = now - static_cast<int64_t>((beat - 1) * beat_interval);
    start_time_ms_.store(new_start);
}

void Metronome::adjustStart(int64_t delta_ms) {
    start_time_ms_.store(start_time_ms_.load() + delta_ms);
}

int64_t Metronome::getTimeOfBeat(int64_t beat) const {
    const double beat_interval = 60000.0 / tempo_.load();
    return start_time_ms_.load() + static_cast<int64_t>((beat - 1) * beat_interval);
}

double Metronome::beatsToMilliseconds(int beats, double tempo) {
    if (tempo <= 0.0) {
        return 0.0;
    }
    return (60000.0 / tempo) * beats;
}

int64_t Metronome::nowMs() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

} // namespace beatlink
