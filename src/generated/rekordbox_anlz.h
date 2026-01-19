#pragma once

/**
 * rekordbox_anlz.h - ANLZ file parser
 *
 * Parses rekordbox track analysis files (.DAT, .EXT, .2EX)
 * Based on Kaitai Struct definition from crate-digger
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <istream>

namespace rekordbox_anlz {

// Forward declarations
class RekordboxAnlz;
class TaggedSection;
class BeatGridTag;
class BeatGridBeat;
class CueTag;
class CueEntry;
class CueExtendedTag;
class CueExtendedEntry;
class PathTag;
class WavePreviewTag;
class WaveScrollTag;
class WaveColorPreviewTag;
class WaveColorScrollTag;
class SongStructureTag;
class SongStructureBody;
class SongStructureEntry;

// =============================================================================
// Enums
// =============================================================================

enum class SectionTags : int32_t {
    CUES = 0x50434f42,           // PCOB
    CUES_2 = 0x50434f32,         // PCO2
    PATH = 0x50505448,           // PPTH
    VBR = 0x50564252,            // PVBR
    BEAT_GRID = 0x5051545a,      // PQTZ
    WAVE_PREVIEW = 0x50574156,   // PWAV
    WAVE_TINY = 0x50575632,      // PWV2
    WAVE_SCROLL = 0x50575633,    // PWV3
    WAVE_COLOR_PREVIEW = 0x50575634, // PWV4
    WAVE_COLOR_SCROLL = 0x50575635,  // PWV5
    SONG_STRUCTURE = 0x50535349,     // PSSI
    WAVE_3BAND_PREVIEW = 0x50575636, // PWV6
    WAVE_3BAND_SCROLL = 0x50575637   // PWV7
};

enum class CueListType : uint32_t {
    MEMORY_CUES = 0,
    HOT_CUES = 1
};

enum class CueEntryType : uint8_t {
    MEMORY_CUE = 1,
    LOOP = 2
};

enum class CueEntryStatus : uint32_t {
    DISABLED = 0,
    ENABLED = 1,
    ACTIVE_LOOP = 4
};

enum class TrackMood : uint16_t {
    HIGH = 1,
    MID = 2,
    LOW = 3
};

enum class MoodHighPhrase : uint16_t {
    INTRO = 1,
    UP = 2,
    DOWN = 3,
    CHORUS = 5,
    OUTRO = 6
};

enum class MoodMidPhrase : uint16_t {
    INTRO = 1,
    VERSE_1 = 2,
    VERSE_2 = 3,
    VERSE_3 = 4,
    VERSE_4 = 5,
    VERSE_5 = 6,
    VERSE_6 = 7,
    BRIDGE = 8,
    CHORUS = 9,
    OUTRO = 10
};

enum class MoodLowPhrase : uint16_t {
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

// =============================================================================
// Data Structures
// =============================================================================

struct BeatGridBeat {
    uint16_t beatNumber;    // Position within bar (1 = downbeat)
    uint16_t tempo;         // BPM * 100
    uint32_t time;          // Time in milliseconds
};

struct CueEntry {
    uint32_t hotCue;        // 0 = memory cue, otherwise hot cue number
    CueEntryStatus status;
    CueEntryType type;
    uint32_t time;          // Position in milliseconds
    uint32_t loopTime;      // Loop end position in milliseconds
};

struct CueExtendedEntry {
    uint32_t hotCue;
    CueEntryType type;
    uint32_t time;
    uint32_t loopTime;
    uint8_t colorId;
    uint16_t loopNumerator;
    uint16_t loopDenominator;
    std::string comment;
    uint8_t colorCode;
    uint8_t colorRed;
    uint8_t colorGreen;
    uint8_t colorBlue;
};

struct SongStructureEntry {
    uint16_t index;         // Phrase number (1-based)
    uint16_t beat;          // Starting beat
    uint16_t kind;          // Phrase type (interpretation depends on mood)
    uint8_t fill;           // Fill-in present at end
    uint16_t beatFill;      // Beat where fill starts
};

// =============================================================================
// Tag Classes
// =============================================================================

class BeatGridTag {
public:
    std::vector<BeatGridBeat> beats;

    static std::unique_ptr<BeatGridTag> parse(const uint8_t* data, size_t size);
};

class CueTag {
public:
    CueListType type;
    std::vector<CueEntry> cues;

    static std::unique_ptr<CueTag> parse(const uint8_t* data, size_t size);
};

class CueExtendedTag {
public:
    CueListType type;
    std::vector<CueExtendedEntry> cues;

    static std::unique_ptr<CueExtendedTag> parse(const uint8_t* data, size_t size);
};

class PathTag {
public:
    std::string path;

    static std::unique_ptr<PathTag> parse(const uint8_t* data, size_t size);
};

class WavePreviewTag {
public:
    std::vector<uint8_t> data;

    static std::unique_ptr<WavePreviewTag> parse(const uint8_t* data, size_t size);
};

class WaveScrollTag {
public:
    uint32_t entrySize;
    std::vector<uint8_t> entries;

    static std::unique_ptr<WaveScrollTag> parse(const uint8_t* data, size_t size);
};

class WaveColorPreviewTag {
public:
    uint32_t entrySize;
    std::vector<uint8_t> entries;

    static std::unique_ptr<WaveColorPreviewTag> parse(const uint8_t* data, size_t size);
};

class WaveColorScrollTag {
public:
    uint32_t entrySize;
    std::vector<uint8_t> entries;

    static std::unique_ptr<WaveColorScrollTag> parse(const uint8_t* data, size_t size);
};

class SongStructureTag {
public:
    TrackMood mood;
    uint16_t endBeat;
    uint8_t bank;
    std::vector<SongStructureEntry> entries;

    static std::unique_ptr<SongStructureTag> parse(const uint8_t* data, size_t size);
};

// =============================================================================
// Tagged Section
// =============================================================================

class TaggedSection {
public:
    SectionTags fourcc;
    uint32_t lenHeader;
    uint32_t lenTag;

    // One of these will be set based on fourcc
    std::unique_ptr<BeatGridTag> beatGrid;
    std::unique_ptr<CueTag> cues;
    std::unique_ptr<CueExtendedTag> cuesExtended;
    std::unique_ptr<PathTag> path;
    std::unique_ptr<WavePreviewTag> wavePreview;
    std::unique_ptr<WaveScrollTag> waveScroll;
    std::unique_ptr<WaveColorPreviewTag> waveColorPreview;
    std::unique_ptr<WaveColorScrollTag> waveColorScroll;
    std::unique_ptr<SongStructureTag> songStructure;
    std::vector<uint8_t> rawData;  // For unknown tags

    static std::unique_ptr<TaggedSection> parse(const uint8_t* data, size_t size, size_t& bytesRead);
};

// =============================================================================
// Main Parser
// =============================================================================

class RekordboxAnlz {
public:
    uint32_t lenHeader;
    uint32_t lenFile;
    std::vector<std::unique_ptr<TaggedSection>> sections;

    /**
     * Parse ANLZ file from memory buffer
     */
    static std::unique_ptr<RekordboxAnlz> parse(const uint8_t* data, size_t size);

    /**
     * Parse ANLZ file from input stream
     */
    static std::unique_ptr<RekordboxAnlz> parse(std::istream& stream);

    /**
     * Parse ANLZ file from file path
     */
    static std::unique_ptr<RekordboxAnlz> parseFile(const std::string& path);

    // Convenience methods
    const BeatGridTag* getBeatGrid() const;
    const CueTag* getMemoryCues() const;
    const CueTag* getHotCues() const;
    const CueExtendedTag* getMemoryCuesExtended() const;
    const CueExtendedTag* getHotCuesExtended() const;
    const PathTag* getPath() const;
    const WavePreviewTag* getWavePreview() const;
    const WaveScrollTag* getWaveScroll() const;
    const WaveColorPreviewTag* getWaveColorPreview() const;
    const WaveColorScrollTag* getWaveColorScroll() const;
    const SongStructureTag* getSongStructure() const;
};

} // namespace rekordbox_anlz
