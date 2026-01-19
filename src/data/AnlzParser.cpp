#include "beatlink/data/AnlzParser.hpp"
#include "beatlink/data/AnlzTypes.hpp"
#include "rekordbox_anlz.h"

#include <filesystem>
#include <fstream>

namespace beatlink::data {

namespace fs = std::filesystem;

// =============================================================================
// Implementation
// =============================================================================

struct AnlzParser::Impl {
    std::unique_ptr<rekordbox_anlz::RekordboxAnlz> anlz;
};

// =============================================================================
// AnlzParser
// =============================================================================

AnlzParser::AnlzParser() : impl_(std::make_unique<Impl>()) {}

AnlzParser::~AnlzParser() = default;

AnlzParser::AnlzParser(AnlzParser&& other) noexcept
    : impl_(std::move(other.impl_)) {}

AnlzParser& AnlzParser::operator=(AnlzParser&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool AnlzParser::parseFile(const std::string& path) {
    impl_->anlz = rekordbox_anlz::RekordboxAnlz::parseFile(path);
    return impl_->anlz != nullptr;
}

bool AnlzParser::parseMemory(const uint8_t* data, size_t size) {
    impl_->anlz = rekordbox_anlz::RekordboxAnlz::parse(data, size);
    return impl_->anlz != nullptr;
}

bool AnlzParser::isValid() const {
    return impl_->anlz != nullptr;
}

std::shared_ptr<BeatGrid> AnlzParser::getBeatGrid() const {
    if (!impl_->anlz) return nullptr;

    const auto* tag = impl_->anlz->getBeatGrid();
    if (!tag || tag->beats.empty()) return nullptr;

    std::vector<BeatGridEntry> entries;
    entries.reserve(tag->beats.size());

    for (const auto& beat : tag->beats) {
        BeatGridEntry entry;
        entry.beatNumber = beat.beatNumber;
        entry.tempo = beat.tempo / 100.0f;  // Convert from BPM*100
        entry.timeMs = beat.time;
        entries.push_back(entry);
    }

    return std::make_shared<BeatGrid>(std::move(entries));
}

std::shared_ptr<CueList> AnlzParser::getMemoryCues() const {
    if (!impl_->anlz) return nullptr;

    // Try extended cues first
    const auto* extTag = impl_->anlz->getMemoryCuesExtended();
    if (extTag && !extTag->cues.empty()) {
        std::vector<CueEntry> entries;
        entries.reserve(extTag->cues.size());

        for (const auto& cue : extTag->cues) {
            CueEntry entry;
            entry.hotCue = cue.hotCue;
            entry.isLoop = (cue.type == rekordbox_anlz::CueEntryType::LOOP);
            entry.timeMs = cue.time;
            entry.loopTimeMs = cue.loopTime;
            entry.comment = cue.comment;
            entry.colorRed = cue.colorRed;
            entry.colorGreen = cue.colorGreen;
            entry.colorBlue = cue.colorBlue;
            entries.push_back(entry);
        }

        return std::make_shared<CueList>(std::move(entries), false);
    }

    // Fall back to basic cues
    const auto* tag = impl_->anlz->getMemoryCues();
    if (!tag || tag->cues.empty()) return nullptr;

    std::vector<CueEntry> entries;
    entries.reserve(tag->cues.size());

    for (const auto& cue : tag->cues) {
        CueEntry entry;
        entry.hotCue = cue.hotCue;
        entry.isLoop = (cue.type == rekordbox_anlz::CueEntryType::LOOP);
        entry.timeMs = cue.time;
        entry.loopTimeMs = cue.loopTime;
        entries.push_back(entry);
    }

    return std::make_shared<CueList>(std::move(entries), false);
}

std::shared_ptr<CueList> AnlzParser::getHotCues() const {
    if (!impl_->anlz) return nullptr;

    // Try extended cues first
    const auto* extTag = impl_->anlz->getHotCuesExtended();
    if (extTag && !extTag->cues.empty()) {
        std::vector<CueEntry> entries;
        entries.reserve(extTag->cues.size());

        for (const auto& cue : extTag->cues) {
            CueEntry entry;
            entry.hotCue = cue.hotCue;
            entry.isLoop = (cue.type == rekordbox_anlz::CueEntryType::LOOP);
            entry.timeMs = cue.time;
            entry.loopTimeMs = cue.loopTime;
            entry.comment = cue.comment;
            entry.colorRed = cue.colorRed;
            entry.colorGreen = cue.colorGreen;
            entry.colorBlue = cue.colorBlue;
            entries.push_back(entry);
        }

        return std::make_shared<CueList>(std::move(entries), true);
    }

    // Fall back to basic cues
    const auto* tag = impl_->anlz->getHotCues();
    if (!tag || tag->cues.empty()) return nullptr;

    std::vector<CueEntry> entries;
    entries.reserve(tag->cues.size());

    for (const auto& cue : tag->cues) {
        CueEntry entry;
        entry.hotCue = cue.hotCue;
        entry.isLoop = (cue.type == rekordbox_anlz::CueEntryType::LOOP);
        entry.timeMs = cue.time;
        entry.loopTimeMs = cue.loopTime;
        entries.push_back(entry);
    }

    return std::make_shared<CueList>(std::move(entries), true);
}

std::shared_ptr<WaveformPreview> AnlzParser::getWaveformPreview() const {
    if (!impl_->anlz) return nullptr;

    const auto* tag = impl_->anlz->getWavePreview();
    if (!tag || tag->data.empty()) return nullptr;

    return std::make_shared<WaveformPreview>(tag->data);
}

std::shared_ptr<WaveformPreview> AnlzParser::getColorWaveformPreview() const {
    if (!impl_->anlz) return nullptr;

    const auto* tag = impl_->anlz->getWaveColorPreview();
    if (!tag || tag->entries.empty()) return nullptr;

    // Color waveform has 6 bytes per entry
    return std::make_shared<WaveformPreview>(tag->entries, tag->entrySize);
}

std::shared_ptr<WaveformDetail> AnlzParser::getWaveformDetail() const {
    if (!impl_->anlz) return nullptr;

    const auto* tag = impl_->anlz->getWaveScroll();
    if (!tag || tag->entries.empty()) return nullptr;

    return std::make_shared<WaveformDetail>(tag->entries, tag->entrySize);
}

std::shared_ptr<WaveformDetail> AnlzParser::getColorWaveformDetail() const {
    if (!impl_->anlz) return nullptr;

    const auto* tag = impl_->anlz->getWaveColorScroll();
    if (!tag || tag->entries.empty()) return nullptr;

    return std::make_shared<WaveformDetail>(tag->entries, tag->entrySize);
}

std::shared_ptr<SongStructure> AnlzParser::getSongStructure() const {
    if (!impl_->anlz) return nullptr;

    const auto* tag = impl_->anlz->getSongStructure();
    if (!tag || tag->entries.empty()) return nullptr;

    SongMood mood;
    switch (tag->mood) {
        case rekordbox_anlz::TrackMood::HIGH:
            mood = SongMood::HIGH;
            break;
        case rekordbox_anlz::TrackMood::MID:
            mood = SongMood::MID;
            break;
        case rekordbox_anlz::TrackMood::LOW:
            mood = SongMood::LOW;
            break;
        default:
            mood = SongMood::MID;
            break;
    }

    std::vector<PhraseEntry> entries;
    entries.reserve(tag->entries.size());

    for (const auto& e : tag->entries) {
        PhraseEntry entry;
        entry.beat = e.beat;
        entry.phraseType = e.kind;
        entry.fill = e.fill;
        entry.fillSize = e.beatFill;
        entries.push_back(entry);
    }

    return std::make_shared<SongStructure>(mood, tag->endBeat, std::move(entries));
}

std::string AnlzParser::getTrackPath() const {
    if (!impl_->anlz) return "";

    const auto* tag = impl_->anlz->getPath();
    if (!tag) return "";

    return tag->path;
}

std::vector<uint8_t> AnlzParser::getRawSection(const std::string& tag) const {
    if (!impl_->anlz) return {};

    // Convert tag string to fourcc
    if (tag.size() != 4) return {};

    int32_t fourcc = (static_cast<int32_t>(tag[0]) << 24) |
                     (static_cast<int32_t>(tag[1]) << 16) |
                     (static_cast<int32_t>(tag[2]) << 8) |
                     static_cast<int32_t>(tag[3]);

    for (const auto& section : impl_->anlz->sections) {
        if (static_cast<int32_t>(section->fourcc) == fourcc) {
            return section->rawData;
        }
    }

    return {};
}

// =============================================================================
// TrackAnalysisFinder
// =============================================================================

std::string TrackAnalysisFinder::findAnalysisPath(const std::string& basePath,
                                                   const std::string& analyzePath,
                                                   AnalysisFileFormat format) {
    if (analyzePath.empty()) return "";

    // analyzePath is typically like "/PIONEER/USBANLZ/P000/0000/ANLZ0000.DAT"
    // We need to construct the full path

    std::string fullPath;

    if (format == AnalysisFileFormat::EXT) {
        // Try .EXT extension
        std::string extPath = analyzePath;
        if (extPath.size() > 4) {
            extPath = extPath.substr(0, extPath.size() - 4) + ".EXT";
        }
        fullPath = basePath + extPath;
        if (fs::exists(fullPath)) {
            return fullPath;
        }

        // Try .2EX extension
        std::string ex2Path = analyzePath;
        if (ex2Path.size() > 4) {
            ex2Path = ex2Path.substr(0, ex2Path.size() - 4) + ".2EX";
        }
        fullPath = basePath + ex2Path;
        if (fs::exists(fullPath)) {
            return fullPath;
        }
    }

    // Try original .DAT
    fullPath = basePath + analyzePath;
    if (fs::exists(fullPath)) {
        return fullPath;
    }

    return "";
}

std::vector<std::shared_ptr<AnlzParser>>
TrackAnalysisFinder::parseAllAnalysisFiles(const std::string& basePath,
                                            const std::string& analyzePath) {
    std::vector<std::shared_ptr<AnlzParser>> results;

    if (analyzePath.empty()) return results;

    // Parse .DAT file
    std::string datPath = basePath + analyzePath;
    if (fs::exists(datPath)) {
        auto parser = std::make_shared<AnlzParser>();
        if (parser->parseFile(datPath)) {
            results.push_back(parser);
        }
    }

    // Parse .EXT file
    std::string extPath = analyzePath;
    if (extPath.size() > 4) {
        extPath = extPath.substr(0, extPath.size() - 4) + ".EXT";
    }
    std::string fullExtPath = basePath + extPath;
    if (fs::exists(fullExtPath)) {
        auto parser = std::make_shared<AnlzParser>();
        if (parser->parseFile(fullExtPath)) {
            results.push_back(parser);
        }
    }

    // Parse .2EX file
    std::string ex2Path = analyzePath;
    if (ex2Path.size() > 4) {
        ex2Path = ex2Path.substr(0, ex2Path.size() - 4) + ".2EX";
    }
    std::string fullEx2Path = basePath + ex2Path;
    if (fs::exists(fullEx2Path)) {
        auto parser = std::make_shared<AnlzParser>();
        if (parser->parseFile(fullEx2Path)) {
            results.push_back(parser);
        }
    }

    return results;
}

} // namespace beatlink::data
