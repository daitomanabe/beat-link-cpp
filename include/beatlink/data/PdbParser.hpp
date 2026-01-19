#pragma once

/**
 * PdbParser - High-level wrapper for rekordbox PDB (DeviceSQL) file parsing
 *
 * Provides a convenient interface for extracting track metadata, artwork,
 * and playlist information from export.pdb files.
 */

#include "beatlink/data/TrackMetadata.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace beatlink::data {

/**
 * Album information from PDB
 */
struct PdbAlbum {
    uint32_t id;
    std::string name;
    uint32_t artistId;
};

/**
 * Artist information from PDB
 */
struct PdbArtist {
    uint32_t id;
    std::string name;
};

/**
 * Genre information from PDB
 */
struct PdbGenre {
    uint32_t id;
    std::string name;
};

/**
 * Artwork information from PDB
 */
struct PdbArtwork {
    uint32_t id;
    std::string path;
};

/**
 * Track information from PDB
 */
struct PdbTrack {
    uint32_t id;
    std::string title;
    std::string path;
    std::string filename;
    std::string comment;
    std::string dateAdded;
    std::string mixName;
    std::string analyzePath;

    uint32_t artistId;
    uint32_t albumId;
    uint32_t genreId;
    uint32_t labelId;
    uint32_t keyId;
    uint16_t artworkId;

    uint32_t duration;       // Duration in seconds
    uint32_t tempo;          // BPM * 100
    uint16_t trackNumber;
    uint16_t discNumber;
    uint16_t year;
    uint8_t rating;
    uint8_t colorId;
    uint16_t playCount;
    uint32_t bitrate;        // In kbps
    uint32_t sampleRate;
    uint32_t fileSize;
};

/**
 * Playlist information from PDB
 */
struct PdbPlaylist {
    uint32_t id;
    uint32_t parentId;
    std::string name;
    uint32_t sortOrder;
    bool isFolder;
};

/**
 * Playlist entry from PDB
 */
struct PdbPlaylistEntry {
    uint32_t playlistId;
    uint32_t trackId;
    uint32_t entryIndex;
};

/**
 * High-level PDB file parser
 */
class PdbParser {
public:
    PdbParser();
    ~PdbParser();

    // Non-copyable, movable
    PdbParser(const PdbParser&) = delete;
    PdbParser& operator=(const PdbParser&) = delete;
    PdbParser(PdbParser&& other) noexcept;
    PdbParser& operator=(PdbParser&& other) noexcept;

    /**
     * Parse PDB file from file path
     * @param path Path to export.pdb or exportExt.pdb
     * @param isExt True if this is exportExt.pdb (extended schema)
     * @return true if successful
     */
    bool parseFile(const std::string& path, bool isExt = false);

    /**
     * Parse PDB file from memory
     * @param data Pointer to file data
     * @param size Size of data
     * @param isExt True if this is exportExt.pdb
     * @return true if successful
     */
    bool parseMemory(const uint8_t* data, size_t size, bool isExt = false);

    /**
     * Check if parsing was successful
     */
    bool isValid() const;

    /**
     * Find track by ID
     * @param id Track ID
     * @return Track data or nullopt if not found
     */
    std::optional<PdbTrack> findTrack(uint32_t id) const;

    /**
     * Find artist by ID
     */
    std::optional<PdbArtist> findArtist(uint32_t id) const;

    /**
     * Find album by ID
     */
    std::optional<PdbAlbum> findAlbum(uint32_t id) const;

    /**
     * Find genre by ID
     */
    std::optional<PdbGenre> findGenre(uint32_t id) const;

    /**
     * Find artwork by ID
     */
    std::optional<PdbArtwork> findArtwork(uint32_t id) const;

    /**
     * Get all tracks
     */
    std::vector<PdbTrack> getAllTracks() const;

    /**
     * Get all artists
     */
    std::vector<PdbArtist> getAllArtists() const;

    /**
     * Get all albums
     */
    std::vector<PdbAlbum> getAllAlbums() const;

    /**
     * Get all genres
     */
    std::vector<PdbGenre> getAllGenres() const;

    /**
     * Get all playlists
     */
    std::vector<PdbPlaylist> getAllPlaylists() const;

    /**
     * Get playlist entries for a playlist
     */
    std::vector<PdbPlaylistEntry> getPlaylistEntries(uint32_t playlistId) const;

    /**
     * Get all artworks
     */
    std::vector<PdbArtwork> getAllArtworks() const;

    /**
     * Iterate over all tracks
     * @param callback Called for each track, return false to stop
     */
    void forEachTrack(std::function<bool(const PdbTrack&)> callback) const;

    /**
     * Convert PdbTrack to TrackMetadata
     * @param track PDB track data
     * @return TrackMetadata instance
     */
    std::shared_ptr<TrackMetadata> toTrackMetadata(const PdbTrack& track) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace beatlink::data
