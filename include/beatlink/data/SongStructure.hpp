#pragma once

/**
 * SongStructure - Phrase analysis data for tracks
 *
 * Ported from: RekordboxAnlz.SongStructureTag (via kaitai-struct)
 *
 * Contains the musical phrase structure of a track, identifying sections
 * like intro, verse, chorus, build-up, drop, outro, etc.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "WaveformPreview.hpp"  // For Color type

namespace beatlink::data {

/**
 * Mood classification for song structure.
 * Determines which phrase type enumeration is used.
 */
enum class SongMood {
    HIGH = 1,   ///< High energy track (uses PhraseHigh types)
    MID = 2,    ///< Mid energy track (uses PhraseMid types)
    LOW = 3     ///< Low energy track (uses PhraseLow types)
};

/**
 * Phrase types for HIGH mood tracks.
 */
enum class PhraseHigh {
    INTRO = 1,
    UP = 2,
    DOWN = 3,
    CHORUS = 5,
    OUTRO = 6
};

/**
 * Phrase types for MID mood tracks.
 */
enum class PhraseMid {
    INTRO = 1,
    VERSE_1 = 2,
    VERSE_2 = 3,
    VERSE_3 = 4,
    VERSE_4 = 5,
    VERSE_5 = 6,
    VERSE_6 = 7,
    CHORUS = 8,
    BRIDGE = 9,
    OUTRO = 10
};

/**
 * Phrase types for LOW mood tracks.
 */
enum class PhraseLow {
    INTRO = 1,
    VERSE_1 = 2,
    VERSE_1B = 3,
    VERSE_1C = 4,
    VERSE_2 = 5,
    VERSE_2B = 6,
    VERSE_2C = 7,
    BRIDGE = 8,
    CHORUS = 9,
    OUTRO = 10
};

/**
 * A single phrase entry in the song structure.
 */
struct PhraseEntry {
    int beat{0};          ///< Starting beat number (1-based)
    int phraseType{0};    ///< Phrase type ID (interpretation depends on mood)
    int fill{0};          ///< Fill-in indicator
    int fillSize{0};      ///< Size of fill-in beats

    /**
     * Get phrase color based on mood and phrase type.
     * Colors match Pioneer CDJ display conventions.
     */
    Color getColor(SongMood mood) const;

    /**
     * Get phrase label text.
     */
    std::string getLabel(SongMood mood) const;
};

/**
 * Song structure containing all phrase information for a track.
 */
class SongStructure {
public:
    SongStructure() = default;

    SongStructure(SongMood mood, int endBeat, std::vector<PhraseEntry> entries)
        : mood_(mood), endBeat_(endBeat), entries_(std::move(entries)) {}

    /**
     * Get the mood classification.
     */
    SongMood getMood() const { return mood_; }

    /**
     * Get the beat number where the song ends.
     */
    int getEndBeat() const { return endBeat_; }

    /**
     * Get the number of phrase entries.
     */
    size_t getEntryCount() const { return entries_.size(); }

    /**
     * Get a specific phrase entry.
     */
    const PhraseEntry& getEntry(size_t index) const { return entries_[index]; }

    /**
     * Get all phrase entries.
     */
    const std::vector<PhraseEntry>& getEntries() const { return entries_; }

    /**
     * Check if this structure is valid/populated.
     */
    bool isValid() const { return !entries_.empty(); }

private:
    SongMood mood_{SongMood::HIGH};
    int endBeat_{0};
    std::vector<PhraseEntry> entries_;
};

// ============================================================================
// Color definitions for phrase types (matching Pioneer CDJ colors)
// ============================================================================

namespace PhraseColors {
    // HIGH mood colors
    constexpr Color HIGH_INTRO{205, 149, 241, 255};    // Light purple
    constexpr Color HIGH_UP{198, 174, 241, 255};       // Purple
    constexpr Color HIGH_DOWN{150, 191, 250, 255};     // Light blue
    constexpr Color HIGH_CHORUS{255, 167, 209, 255};   // Pink
    constexpr Color HIGH_OUTRO{230, 186, 202, 255};    // Light pink

    // MID mood colors
    constexpr Color MID_INTRO{164, 209, 255, 255};     // Light blue
    constexpr Color MID_VERSE_1{133, 226, 204, 255};   // Teal
    constexpr Color MID_VERSE_2{134, 212, 147, 255};   // Green
    constexpr Color MID_VERSE_3{198, 218, 126, 255};   // Yellow-green
    constexpr Color MID_VERSE_4{236, 214, 134, 255};   // Yellow
    constexpr Color MID_VERSE_5{239, 190, 130, 255};   // Orange
    constexpr Color MID_VERSE_6{239, 166, 131, 255};   // Light orange
    constexpr Color MID_CHORUS{230, 141, 160, 255};    // Pink
    constexpr Color MID_BRIDGE{186, 166, 224, 255};    // Light purple
    constexpr Color MID_OUTRO{199, 153, 192, 255};     // Mauve

    // LOW mood colors
    constexpr Color LOW_INTRO{255, 206, 152, 255};     // Light orange
    constexpr Color LOW_VERSE_1{255, 180, 152, 255};   // Peach
    constexpr Color LOW_VERSE_1B{244, 183, 198, 255};  // Pink
    constexpr Color LOW_VERSE_1C{246, 158, 152, 255};  // Salmon
    constexpr Color LOW_VERSE_2{163, 198, 241, 255};   // Blue
    constexpr Color LOW_VERSE_2B{186, 194, 241, 255};  // Light blue
    constexpr Color LOW_VERSE_2C{179, 183, 228, 255};  // Pale blue
    constexpr Color LOW_BRIDGE{160, 203, 185, 255};    // Teal
    constexpr Color LOW_CHORUS{213, 227, 125, 255};    // Lime
    constexpr Color LOW_OUTRO{241, 231, 166, 255};     // Pale yellow
}

// ============================================================================
// Implementation of inline methods
// ============================================================================

inline Color PhraseEntry::getColor(SongMood mood) const {
    switch (mood) {
        case SongMood::HIGH:
            switch (static_cast<PhraseHigh>(phraseType)) {
                case PhraseHigh::INTRO: return PhraseColors::HIGH_INTRO;
                case PhraseHigh::UP: return PhraseColors::HIGH_UP;
                case PhraseHigh::DOWN: return PhraseColors::HIGH_DOWN;
                case PhraseHigh::CHORUS: return PhraseColors::HIGH_CHORUS;
                case PhraseHigh::OUTRO: return PhraseColors::HIGH_OUTRO;
                default: return Color{255, 255, 255, 255};
            }
        case SongMood::MID:
            switch (static_cast<PhraseMid>(phraseType)) {
                case PhraseMid::INTRO: return PhraseColors::MID_INTRO;
                case PhraseMid::VERSE_1: return PhraseColors::MID_VERSE_1;
                case PhraseMid::VERSE_2: return PhraseColors::MID_VERSE_2;
                case PhraseMid::VERSE_3: return PhraseColors::MID_VERSE_3;
                case PhraseMid::VERSE_4: return PhraseColors::MID_VERSE_4;
                case PhraseMid::VERSE_5: return PhraseColors::MID_VERSE_5;
                case PhraseMid::VERSE_6: return PhraseColors::MID_VERSE_6;
                case PhraseMid::CHORUS: return PhraseColors::MID_CHORUS;
                case PhraseMid::BRIDGE: return PhraseColors::MID_BRIDGE;
                case PhraseMid::OUTRO: return PhraseColors::MID_OUTRO;
                default: return Color{255, 255, 255, 255};
            }
        case SongMood::LOW:
            switch (static_cast<PhraseLow>(phraseType)) {
                case PhraseLow::INTRO: return PhraseColors::LOW_INTRO;
                case PhraseLow::VERSE_1: return PhraseColors::LOW_VERSE_1;
                case PhraseLow::VERSE_1B: return PhraseColors::LOW_VERSE_1B;
                case PhraseLow::VERSE_1C: return PhraseColors::LOW_VERSE_1C;
                case PhraseLow::VERSE_2: return PhraseColors::LOW_VERSE_2;
                case PhraseLow::VERSE_2B: return PhraseColors::LOW_VERSE_2B;
                case PhraseLow::VERSE_2C: return PhraseColors::LOW_VERSE_2C;
                case PhraseLow::BRIDGE: return PhraseColors::LOW_BRIDGE;
                case PhraseLow::CHORUS: return PhraseColors::LOW_CHORUS;
                case PhraseLow::OUTRO: return PhraseColors::LOW_OUTRO;
                default: return Color{255, 255, 255, 255};
            }
        default:
            return Color{255, 255, 255, 255};
    }
}

inline std::string PhraseEntry::getLabel(SongMood mood) const {
    switch (mood) {
        case SongMood::HIGH:
            switch (static_cast<PhraseHigh>(phraseType)) {
                case PhraseHigh::INTRO: return "Intro";
                case PhraseHigh::UP: return "Up";
                case PhraseHigh::DOWN: return "Down";
                case PhraseHigh::CHORUS: return "Chorus";
                case PhraseHigh::OUTRO: return "Outro";
                default: return "Unknown";
            }
        case SongMood::MID:
            switch (static_cast<PhraseMid>(phraseType)) {
                case PhraseMid::INTRO: return "Intro";
                case PhraseMid::VERSE_1: return "Verse 1";
                case PhraseMid::VERSE_2: return "Verse 2";
                case PhraseMid::VERSE_3: return "Verse 3";
                case PhraseMid::VERSE_4: return "Verse 4";
                case PhraseMid::VERSE_5: return "Verse 5";
                case PhraseMid::VERSE_6: return "Verse 6";
                case PhraseMid::CHORUS: return "Chorus";
                case PhraseMid::BRIDGE: return "Bridge";
                case PhraseMid::OUTRO: return "Outro";
                default: return "Unknown";
            }
        case SongMood::LOW:
            switch (static_cast<PhraseLow>(phraseType)) {
                case PhraseLow::INTRO: return "Intro";
                case PhraseLow::VERSE_1: return "Verse 1";
                case PhraseLow::VERSE_1B: return "Verse 1b";
                case PhraseLow::VERSE_1C: return "Verse 1c";
                case PhraseLow::VERSE_2: return "Verse 2";
                case PhraseLow::VERSE_2B: return "Verse 2b";
                case PhraseLow::VERSE_2C: return "Verse 2c";
                case PhraseLow::BRIDGE: return "Bridge";
                case PhraseLow::CHORUS: return "Chorus";
                case PhraseLow::OUTRO: return "Outro";
                default: return "Unknown";
            }
        default:
            return "Unknown";
    }
}

} // namespace beatlink::data
