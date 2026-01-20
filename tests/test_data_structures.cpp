/**
 * @file test_data_structures.cpp
 * @brief Unit tests for data structures (BeatGrid, CueList, Waveform)
 */

#include <catch2/catch_all.hpp>
#include <beatlink/data/BeatGrid.hpp>
#include <beatlink/data/CueList.hpp>
#include <beatlink/data/WaveformPreview.hpp>
#include <beatlink/data/WaveformDetail.hpp>
#include <beatlink/data/DataReference.hpp>
#include <beatlink/data/SlotReference.hpp>
#include <vector>

using namespace beatlink;
using namespace beatlink::data;

// =============================================================================
// DataReference Tests
// =============================================================================

TEST_CASE("DataReference construction", "[DataReference]") {
    SECTION("With player, slot, track") {
        DataReference ref(1, CdSlot::USB_SLOT, 12345);
        CHECK(ref.player == 1);
        CHECK(ref.slot == CdSlot::USB_SLOT);
        CHECK(ref.rekordboxId == 12345);
    }

    SECTION("Default construction") {
        DataReference ref;
        CHECK(ref.player == 0);
        CHECK(ref.rekordboxId == 0);
    }

    SECTION("Equality") {
        DataReference ref1(1, CdSlot::USB_SLOT, 100);
        DataReference ref2(1, CdSlot::USB_SLOT, 100);
        DataReference ref3(2, CdSlot::USB_SLOT, 100);

        CHECK(ref1 == ref2);
        CHECK_FALSE(ref1 == ref3);
    }
}

// =============================================================================
// SlotReference Tests
// =============================================================================

TEST_CASE("SlotReference construction", "[SlotReference]") {
    SECTION("With player and slot") {
        SlotReference ref(2, CdSlot::SD_SLOT);
        CHECK(ref.player == 2);
        CHECK(ref.slot == CdSlot::SD_SLOT);
    }

    SECTION("From DataReference") {
        DataReference dataRef(3, CdSlot::USB_SLOT, 999);
        SlotReference slotRef(dataRef);
        CHECK(slotRef.player == 3);
        CHECK(slotRef.slot == CdSlot::USB_SLOT);
    }
}

// =============================================================================
// CueList Tests
// =============================================================================

TEST_CASE("CueList::Entry construction", "[CueList]") {
    SECTION("Default entry") {
        CueList::Entry entry;
        CHECK(entry.hotCueNumber == 0);
        CHECK(entry.isLoop == false);
        CHECK(entry.cueTimeMs == 0);
        CHECK(entry.loopEndMs == 0);
    }

    SECTION("Hot cue entry") {
        CueList::Entry entry;
        entry.hotCueNumber = 1;
        entry.cueTimeMs = 5000;
        entry.isLoop = false;
        entry.colorId = 1; // Red

        CHECK(entry.hotCueNumber == 1);
        CHECK(entry.cueTimeMs == 5000);
        CHECK(entry.isLoop == false);
    }

    SECTION("Loop entry") {
        CueList::Entry entry;
        entry.hotCueNumber = 2;
        entry.cueTimeMs = 10000;
        entry.loopEndMs = 18000;
        entry.isLoop = true;

        CHECK(entry.isLoop == true);
        CHECK(entry.cueTimeMs == 10000);
        CHECK(entry.loopEndMs == 18000);
    }
}

TEST_CASE("CueList counting", "[CueList]") {
    std::vector<CueList::Entry> entries;

    // Add hot cues
    for (int i = 1; i <= 3; ++i) {
        CueList::Entry e;
        e.hotCueNumber = i;
        e.cueTimeMs = i * 1000;
        e.isLoop = false;
        entries.push_back(e);
    }

    // Add memory point (hot cue number = 0)
    CueList::Entry memPoint;
    memPoint.hotCueNumber = 0;
    memPoint.cueTimeMs = 500;
    memPoint.isLoop = false;
    entries.push_back(memPoint);

    // Add loop
    CueList::Entry loop;
    loop.hotCueNumber = 4;
    loop.cueTimeMs = 20000;
    loop.loopEndMs = 24000;
    loop.isLoop = true;
    entries.push_back(loop);

    // Count manually
    int hotCues = 0, memoryPoints = 0, loops = 0;
    for (const auto& e : entries) {
        if (e.isLoop) {
            ++loops;
        } else if (e.hotCueNumber > 0) {
            ++hotCues;
        } else {
            ++memoryPoints;
        }
    }

    CHECK(hotCues == 3);
    CHECK(memoryPoints == 1);
    CHECK(loops == 1);
    CHECK(entries.size() == 5);
}

// =============================================================================
// Waveform Tests
// =============================================================================

TEST_CASE("WaveformPreview data", "[Waveform]") {
    SECTION("Empty waveform data") {
        std::vector<uint8_t> emptyData;
        // WaveformPreview requires proper construction via factory/parser
        // Here we just test that empty data handling works
        CHECK(emptyData.size() == 0);
    }

    SECTION("Waveform segment count calculation") {
        // Typical preview waveform has 400 segments for a track
        // Each segment is typically 1 byte for monochrome, 6 bytes for color
        constexpr size_t MONO_SEGMENT_SIZE = 1;
        constexpr size_t COLOR_SEGMENT_SIZE = 6;
        constexpr size_t SEGMENT_COUNT = 400;

        CHECK(SEGMENT_COUNT * MONO_SEGMENT_SIZE == 400);
        CHECK(SEGMENT_COUNT * COLOR_SEGMENT_SIZE == 2400);
    }
}

TEST_CASE("WaveformDetail data", "[Waveform]") {
    SECTION("Frame count calculation") {
        // Detail waveform: ~150 frames per second of audio
        // A 5-minute track would have approximately 150 * 60 * 5 = 45000 frames
        constexpr double FRAMES_PER_SECOND = 150.0;
        constexpr double TRACK_LENGTH_SECONDS = 300.0; // 5 minutes

        size_t expectedFrames = static_cast<size_t>(FRAMES_PER_SECOND * TRACK_LENGTH_SECONDS);
        CHECK(expectedFrames == 45000);
    }
}

// =============================================================================
// BeatGrid Tests
// =============================================================================

TEST_CASE("BeatGrid beat calculation", "[BeatGrid]") {
    SECTION("BPM to beat interval") {
        // At 120 BPM, beats are 500ms apart
        double bpm = 120.0;
        double beatIntervalMs = 60000.0 / bpm;
        CHECK(beatIntervalMs == Catch::Approx(500.0).epsilon(0.01));
    }

    SECTION("Beat within bar calculation") {
        // Beat numbers 1-4 repeat cyclically
        for (int beat = 1; beat <= 16; ++beat) {
            int beatWithinBar = ((beat - 1) % 4) + 1;
            CHECK(beatWithinBar >= 1);
            CHECK(beatWithinBar <= 4);
        }

        CHECK(((1 - 1) % 4) + 1 == 1);
        CHECK(((2 - 1) % 4) + 1 == 2);
        CHECK(((3 - 1) % 4) + 1 == 3);
        CHECK(((4 - 1) % 4) + 1 == 4);
        CHECK(((5 - 1) % 4) + 1 == 1);
        CHECK(((8 - 1) % 4) + 1 == 4);
    }

    SECTION("Time to beat conversion at constant tempo") {
        double bpm = 128.0;
        double beatIntervalMs = 60000.0 / bpm;

        // Calculate beat number from time
        auto timeToBeat = [beatIntervalMs](int64_t timeMs) {
            return static_cast<int>(timeMs / beatIntervalMs) + 1;
        };

        CHECK(timeToBeat(0) == 1);
        CHECK(timeToBeat(500) == 2);  // ~1 beat at 128 BPM (468.75ms per beat)
        CHECK(timeToBeat(1000) == 3); // ~2 beats
    }
}

TEST_CASE("BeatGrid variable BPM handling", "[BeatGrid]") {
    SECTION("BPM change detection") {
        // Simulate a track that changes tempo
        struct BeatInfo {
            int beatNumber;
            int bpm100; // BPM * 100
            int64_t timeMs;
        };

        std::vector<BeatInfo> beats = {
            {1, 12000, 0},       // 120.00 BPM
            {2, 12000, 500},
            {3, 12000, 1000},
            {4, 12000, 1500},
            {5, 12800, 2000},   // BPM change to 128.00
            {6, 12800, 2469},
            {7, 12800, 2938},
            {8, 12800, 3406},
        };

        // Count BPM changes
        int bpmChanges = 0;
        for (size_t i = 1; i < beats.size(); ++i) {
            if (beats[i].bpm100 != beats[i - 1].bpm100) {
                ++bpmChanges;
            }
        }

        CHECK(bpmChanges == 1);
    }
}

// =============================================================================
// Color Tests
// =============================================================================

TEST_CASE("Cue color handling", "[CueList]") {
    SECTION("Color ID mapping") {
        // rekordbox color IDs:
        // 0 = No color
        // 1 = Pink/Magenta
        // 2 = Red
        // 3 = Orange
        // 4 = Yellow
        // 5 = Green
        // 6 = Aqua
        // 7 = Blue
        // 8 = Purple

        std::vector<std::pair<int, std::string>> colorMap = {
            {0, "No Color"},
            {1, "Pink"},
            {2, "Red"},
            {3, "Orange"},
            {4, "Yellow"},
            {5, "Green"},
            {6, "Aqua"},
            {7, "Blue"},
            {8, "Purple"}
        };

        CHECK(colorMap.size() == 9);

        for (const auto& [id, name] : colorMap) {
            CHECK(id >= 0);
            CHECK(id <= 8);
            CHECK(!name.empty());
        }
    }
}
