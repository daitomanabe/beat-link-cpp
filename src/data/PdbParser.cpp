#include "beatlink/data/PdbParser.hpp"
#include "rekordbox_pdb.h"

#include <unordered_map>

namespace beatlink::data {

// =============================================================================
// Implementation
// =============================================================================

struct PdbParser::Impl {
    std::unique_ptr<rekordbox_pdb::RekordboxPdb> pdb;

    // Caches for lookup
    mutable std::unordered_map<uint32_t, PdbArtist> artistCache;
    mutable std::unordered_map<uint32_t, PdbAlbum> albumCache;
    mutable std::unordered_map<uint32_t, PdbGenre> genreCache;
    mutable bool cachePopulated = false;

    void populateCache() const {
        if (cachePopulated || !pdb) return;

        // Cache artists
        pdb->forEachPage(rekordbox_pdb::PageType::ARTISTS, [this](const rekordbox_pdb::Page& page) {
            for (const auto& row : page.artists) {
                PdbArtist artist;
                artist.id = row.id;
                artist.name = row.name;
                artistCache[artist.id] = artist;
            }
            return true;
        });

        // Cache albums
        pdb->forEachPage(rekordbox_pdb::PageType::ALBUMS, [this](const rekordbox_pdb::Page& page) {
            for (const auto& row : page.albums) {
                PdbAlbum album;
                album.id = row.id;
                album.name = row.name;
                album.artistId = row.artistId;
                albumCache[album.id] = album;
            }
            return true;
        });

        // Cache genres
        pdb->forEachPage(rekordbox_pdb::PageType::GENRES, [this](const rekordbox_pdb::Page& page) {
            for (const auto& row : page.genres) {
                PdbGenre genre;
                genre.id = row.id;
                genre.name = row.name;
                genreCache[genre.id] = genre;
            }
            return true;
        });

        cachePopulated = true;
    }
};

// =============================================================================
// PdbParser
// =============================================================================

PdbParser::PdbParser() : impl_(std::make_unique<Impl>()) {}

PdbParser::~PdbParser() = default;

PdbParser::PdbParser(PdbParser&& other) noexcept
    : impl_(std::move(other.impl_)) {}

PdbParser& PdbParser::operator=(PdbParser&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool PdbParser::parseFile(const std::string& path, bool isExt) {
    impl_->pdb = rekordbox_pdb::RekordboxPdb::parseFile(path, isExt);
    impl_->cachePopulated = false;
    impl_->artistCache.clear();
    impl_->albumCache.clear();
    impl_->genreCache.clear();
    return impl_->pdb != nullptr;
}

bool PdbParser::parseMemory(const uint8_t* data, size_t size, bool isExt) {
    impl_->pdb = rekordbox_pdb::RekordboxPdb::parse(data, size, isExt);
    impl_->cachePopulated = false;
    impl_->artistCache.clear();
    impl_->albumCache.clear();
    impl_->genreCache.clear();
    return impl_->pdb != nullptr;
}

bool PdbParser::isValid() const {
    return impl_->pdb != nullptr;
}

static PdbTrack convertTrackRow(const rekordbox_pdb::TrackRow& row) {
    PdbTrack track;
    track.id = row.id;
    track.title = row.title;
    track.path = row.isoPath;
    track.filename = row.filename;
    track.comment = row.comment;
    track.dateAdded = row.dateAdded;
    track.mixName = row.mixName;
    track.analyzePath = row.analyzePath;

    track.artistId = row.artistId;
    track.albumId = row.albumId;
    track.genreId = row.genreId;
    track.labelId = row.labelId;
    track.keyId = row.keyId;
    track.artworkId = row.artworkId;

    track.duration = row.duration;
    track.tempo = row.tempo;
    track.trackNumber = row.trackNumber;
    track.discNumber = row.discNumber;
    track.year = row.year;
    track.rating = row.rating;
    track.colorId = row.colorId;
    track.playCount = row.playCount;
    track.bitrate = row.bitrateKbps;
    track.sampleRate = row.sampleRate;
    track.fileSize = row.fileSize;

    return track;
}

std::optional<PdbTrack> PdbParser::findTrack(uint32_t id) const {
    if (!impl_->pdb) return std::nullopt;

    auto row = impl_->pdb->findTrack(id);
    if (!row) return std::nullopt;

    return convertTrackRow(*row);
}

std::optional<PdbArtist> PdbParser::findArtist(uint32_t id) const {
    if (!impl_->pdb) return std::nullopt;

    impl_->populateCache();
    auto it = impl_->artistCache.find(id);
    if (it != impl_->artistCache.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<PdbAlbum> PdbParser::findAlbum(uint32_t id) const {
    if (!impl_->pdb) return std::nullopt;

    impl_->populateCache();
    auto it = impl_->albumCache.find(id);
    if (it != impl_->albumCache.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<PdbGenre> PdbParser::findGenre(uint32_t id) const {
    if (!impl_->pdb) return std::nullopt;

    impl_->populateCache();
    auto it = impl_->genreCache.find(id);
    if (it != impl_->genreCache.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<PdbArtwork> PdbParser::findArtwork(uint32_t id) const {
    if (!impl_->pdb) return std::nullopt;

    auto row = impl_->pdb->findArtwork(id);
    if (!row) return std::nullopt;

    PdbArtwork artwork;
    artwork.id = row->id;
    artwork.path = row->path;
    return artwork;
}

std::vector<PdbTrack> PdbParser::getAllTracks() const {
    std::vector<PdbTrack> result;
    if (!impl_->pdb) return result;

    auto rows = impl_->pdb->getAllTracks();
    result.reserve(rows.size());

    for (const auto& row : rows) {
        result.push_back(convertTrackRow(row));
    }

    return result;
}

std::vector<PdbArtist> PdbParser::getAllArtists() const {
    std::vector<PdbArtist> result;
    if (!impl_->pdb) return result;

    impl_->populateCache();
    result.reserve(impl_->artistCache.size());

    for (const auto& [id, artist] : impl_->artistCache) {
        result.push_back(artist);
    }

    return result;
}

std::vector<PdbAlbum> PdbParser::getAllAlbums() const {
    std::vector<PdbAlbum> result;
    if (!impl_->pdb) return result;

    impl_->populateCache();
    result.reserve(impl_->albumCache.size());

    for (const auto& [id, album] : impl_->albumCache) {
        result.push_back(album);
    }

    return result;
}

std::vector<PdbGenre> PdbParser::getAllGenres() const {
    std::vector<PdbGenre> result;
    if (!impl_->pdb) return result;

    impl_->populateCache();
    result.reserve(impl_->genreCache.size());

    for (const auto& [id, genre] : impl_->genreCache) {
        result.push_back(genre);
    }

    return result;
}

std::vector<PdbPlaylist> PdbParser::getAllPlaylists() const {
    std::vector<PdbPlaylist> result;
    if (!impl_->pdb) return result;

    impl_->pdb->forEachPage(rekordbox_pdb::PageType::PLAYLIST_TREE,
                            [&result](const rekordbox_pdb::Page& page) {
        for (const auto& row : page.playlistTrees) {
            PdbPlaylist playlist;
            playlist.id = row.id;
            playlist.parentId = row.parentId;
            playlist.name = row.name;
            playlist.sortOrder = row.sortOrder;
            playlist.isFolder = row.isFolder();
            result.push_back(playlist);
        }
        return true;
    });

    return result;
}

std::vector<PdbPlaylistEntry> PdbParser::getPlaylistEntries(uint32_t playlistId) const {
    std::vector<PdbPlaylistEntry> result;
    if (!impl_->pdb) return result;

    impl_->pdb->forEachPage(rekordbox_pdb::PageType::PLAYLIST_ENTRIES,
                            [&result, playlistId](const rekordbox_pdb::Page& page) {
        for (const auto& row : page.playlistEntries) {
            if (row.playlistId == playlistId) {
                PdbPlaylistEntry entry;
                entry.playlistId = row.playlistId;
                entry.trackId = row.trackId;
                entry.entryIndex = row.entryIndex;
                result.push_back(entry);
            }
        }
        return true;
    });

    return result;
}

std::vector<PdbArtwork> PdbParser::getAllArtworks() const {
    std::vector<PdbArtwork> result;
    if (!impl_->pdb) return result;

    auto rows = impl_->pdb->getAllArtworks();
    result.reserve(rows.size());

    for (const auto& row : rows) {
        PdbArtwork artwork;
        artwork.id = row.id;
        artwork.path = row.path;
        result.push_back(artwork);
    }

    return result;
}

void PdbParser::forEachTrack(std::function<bool(const PdbTrack&)> callback) const {
    if (!impl_->pdb) return;

    impl_->pdb->forEachPage(rekordbox_pdb::PageType::TRACKS,
                            [&callback](const rekordbox_pdb::Page& page) {
        for (const auto& row : page.tracks) {
            if (!callback(convertTrackRow(row))) {
                return false;
            }
        }
        return true;
    });
}

std::shared_ptr<TrackMetadata> PdbParser::toTrackMetadata(const PdbTrack& track) const {
    // Create DataReference (using slot 0 for local/USB, rekordboxId = track.id)
    DataReference ref(0, TrackSourceSlot::USB_SLOT, static_cast<int>(track.id));

    // Build metadata using the Builder pattern
    TrackMetadata::Builder builder;
    builder.setTrackReference(std::move(ref))
           .setTrackType(TrackType::REKORDBOX)
           .setTitle(track.title)
           .setComment(track.comment)
           .setDateAdded(track.dateAdded)
           .setDuration(static_cast<int>(track.duration))
           .setTempo(static_cast<int>(track.tempo))
           .setRating(static_cast<int>(track.rating))
           .setYear(static_cast<int>(track.year))
           .setBitRate(static_cast<int>(track.bitrate))
           .setArtworkId(static_cast<int>(track.artworkId));

    // Look up artist name
    if (track.artistId != 0) {
        auto artist = findArtist(track.artistId);
        if (artist) {
            builder.setArtist(artist->name, static_cast<int>(artist->id));
        }
    }

    // Look up album name
    if (track.albumId != 0) {
        auto album = findAlbum(track.albumId);
        if (album) {
            builder.setAlbum(album->name, static_cast<int>(album->id));
        }
    }

    // Look up genre name
    if (track.genreId != 0) {
        auto genre = findGenre(track.genreId);
        if (genre) {
            builder.setGenre(genre->name, static_cast<int>(genre->id));
        }
    }

    // Set color if available (colorId maps to rekordbox color codes)
    if (track.colorId != 0) {
        builder.setColor(static_cast<int>(track.colorId));
    }

    return builder.build();
}

} // namespace beatlink::data
