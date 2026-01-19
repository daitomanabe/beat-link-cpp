#pragma once

/**
 * Types for ANLZ file parsing
 *
 * These structures are used by AnlzParser to represent data
 * parsed from ANLZ analysis files.
 */

#include <cstdint>
#include <string>

namespace beatlink::data {

/**
 * Cue entry from ANLZ file
 */
struct CueEntry {
    int hotCue{0};              // Hot cue number (0 for memory cue)
    bool isLoop{false};         // Whether this is a loop
    int64_t timeMs{0};          // Position in milliseconds
    int64_t loopTimeMs{0};      // Loop end position (if isLoop)
    std::string comment;        // Cue label/comment
    uint8_t colorRed{0};        // Embedded color red component
    uint8_t colorGreen{0};      // Embedded color green component
    uint8_t colorBlue{0};       // Embedded color blue component
};

/**
 * Beat grid entry from ANLZ file
 */
struct BeatGridEntry {
    int beatNumber{0};          // Beat number within bar (1-4)
    float tempo{0.0f};          // BPM at this beat
    int64_t timeMs{0};          // Position in milliseconds
};

// Note: PhraseEntry and SongMood are defined in SongStructure.hpp

} // namespace beatlink::data
