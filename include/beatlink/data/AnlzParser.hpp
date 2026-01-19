#pragma once

/**
 * AnlzParser - High-level wrapper for rekordbox ANLZ file parsing
 *
 * Provides a convenient interface for extracting beat grids, cue lists,
 * waveforms, and song structure from ANLZ analysis files.
 */

#include "beatlink/data/BeatGrid.hpp"
#include "beatlink/data/CueList.hpp"
#include "beatlink/data/SongStructure.hpp"
#include "beatlink/data/WaveformPreview.hpp"
#include "beatlink/data/WaveformDetail.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace beatlink::data {

/**
 * Analysis file format (affects file naming and location)
 */
enum class AnalysisFileFormat {
    EXT,      // Extended format (.EXT files)
    PIONEER   // Original Pioneer format (PIONEER/USBANLZ)
};

/**
 * High-level ANLZ file parser
 */
class AnlzParser {
public:
    AnlzParser();
    ~AnlzParser();

    // Non-copyable, movable
    AnlzParser(const AnlzParser&) = delete;
    AnlzParser& operator=(const AnlzParser&) = delete;
    AnlzParser(AnlzParser&& other) noexcept;
    AnlzParser& operator=(AnlzParser&& other) noexcept;

    /**
     * Parse ANLZ file from file path
     * @param path Path to .DAT, .EXT, or .2EX file
     * @return true if successful
     */
    bool parseFile(const std::string& path);

    /**
     * Parse ANLZ file from memory
     * @param data Pointer to file data
     * @param size Size of data
     * @return true if successful
     */
    bool parseMemory(const uint8_t* data, size_t size);

    /**
     * Check if parsing was successful
     */
    bool isValid() const;

    /**
     * Get beat grid data
     * @return BeatGrid or nullptr if not available
     */
    std::shared_ptr<BeatGrid> getBeatGrid() const;

    /**
     * Get memory cue list
     * @return CueList or nullptr if not available
     */
    std::shared_ptr<CueList> getMemoryCues() const;

    /**
     * Get hot cue list
     * @return CueList or nullptr if not available
     */
    std::shared_ptr<CueList> getHotCues() const;

    /**
     * Get waveform preview data
     * @return WaveformPreview or nullptr if not available
     */
    std::shared_ptr<WaveformPreview> getWaveformPreview() const;

    /**
     * Get color waveform preview (from .EXT)
     * @return WaveformPreview or nullptr if not available
     */
    std::shared_ptr<WaveformPreview> getColorWaveformPreview() const;

    /**
     * Get waveform detail data
     * @return WaveformDetail or nullptr if not available
     */
    std::shared_ptr<WaveformDetail> getWaveformDetail() const;

    /**
     * Get color waveform detail (from .EXT)
     * @return WaveformDetail or nullptr if not available
     */
    std::shared_ptr<WaveformDetail> getColorWaveformDetail() const;

    /**
     * Get song structure (phrase analysis)
     * @return SongStructure or nullptr if not available
     */
    std::shared_ptr<SongStructure> getSongStructure() const;

    /**
     * Get track path stored in the ANLZ file
     */
    std::string getTrackPath() const;

    /**
     * Get raw section data by tag
     * @param tag Section tag (e.g., "PQTZ", "PSSI")
     * @return Raw section data or empty vector if not found
     */
    std::vector<uint8_t> getRawSection(const std::string& tag) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * Find and parse track analysis files
 *
 * Searches for .DAT, .EXT, and .2EX files in the expected locations
 * based on the track's analyze path.
 */
class TrackAnalysisFinder {
public:
    /**
     * Find analysis file path for a track
     * @param basePath Base path of the media (USB root)
     * @param analyzePath Analyze path from track metadata
     * @param format Analysis file format to look for
     * @return Full path to analysis file, or empty if not found
     */
    static std::string findAnalysisPath(const std::string& basePath,
                                         const std::string& analyzePath,
                                         AnalysisFileFormat format);

    /**
     * Parse all available analysis files for a track
     * @param basePath Base path of the media
     * @param analyzePath Analyze path from track metadata
     * @return Vector of parsed analysis files (DAT, EXT, 2EX)
     */
    static std::vector<std::shared_ptr<AnlzParser>>
    parseAllAnalysisFiles(const std::string& basePath,
                          const std::string& analyzePath);
};

} // namespace beatlink::data
