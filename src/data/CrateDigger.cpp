#include "beatlink/data/CrateDigger.hpp"

#include "beatlink/DeviceFinder.hpp"
#include "beatlink/VirtualCdj.hpp"
#include "beatlink/data/MetadataFinder.hpp"
#include "beatlink/data/OpusProvider.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <iostream>

namespace beatlink::data {

namespace {
class CrateDiggerProvider final : public MetadataProvider {
public:
    std::vector<beatlink::MediaDetails> supportedMedia() const override { return {}; }

    std::shared_ptr<TrackMetadata> getTrackMetadata(const beatlink::MediaDetails&,
                                                    const DataReference&) override {
        return nullptr;
    }

    std::shared_ptr<AlbumArt> getAlbumArt(const beatlink::MediaDetails&,
                                          const DataReference&) override {
        return nullptr;
    }

    std::shared_ptr<BeatGrid> getBeatGrid(const beatlink::MediaDetails&,
                                          const DataReference&) override {
        return nullptr;
    }

    std::shared_ptr<CueList> getCueList(const beatlink::MediaDetails&,
                                        const DataReference&) override {
        return nullptr;
    }

    std::shared_ptr<WaveformPreview> getWaveformPreview(const beatlink::MediaDetails&,
                                                        const DataReference&) override {
        return nullptr;
    }

    std::shared_ptr<WaveformDetail> getWaveformDetail(const beatlink::MediaDetails&,
                                                      const DataReference&) override {
        return nullptr;
    }

    std::shared_ptr<AnalysisTag> getAnalysisSection(const beatlink::MediaDetails&,
                                                    const DataReference&,
                                                    const std::string&,
                                                    const std::string&) override {
        return nullptr;
    }
};
} // namespace

CrateDigger& CrateDigger::getInstance() {
    static CrateDigger instance;
    return instance;
}

CrateDigger::CrateDigger() {
    downloadDirectory_ = createDownloadDirectory();

    metadataProvider_ = std::make_shared<CrateDiggerProvider>();

    mountListener_ = std::make_shared<MountCallbacks>(
        [](const SlotReference&) {},
        [this](const SlotReference& slot) {
            std::shared_ptr<Database> database;
            std::shared_ptr<SQLiteConnection> connection;
            {
                std::lock_guard<std::mutex> lock(databasesMutex_);
                auto it = databases_.find(slot);
                if (it != databases_.end()) {
                    database = it->second;
                    databases_.erase(it);
                }
            }
            {
                std::lock_guard<std::mutex> lock(sqliteMutex_);
                auto it = sqliteConnections_.find(slot);
                if (it != sqliteConnections_.end()) {
                    connection = it->second;
                    sqliteConnections_.erase(it);
                }
            }
            if (database) {
                deliverDatabaseUpdate(slot, database, false);
            }
            if (connection) {
                deliverConnectionUpdate(slot, connection, false);
            }
        });

    mediaDetailsListener_ = std::make_shared<beatlink::MediaDetailsCallbacks>(
        [this](const beatlink::MediaDetails& details) {
            if (!isRunning()) {
                return;
            }
            if (details.getMediaType() != beatlink::TrackType::REKORDBOX) {
                return;
            }
            const auto& slot = details.getSlotReference();
            if (slot.getSlot() == beatlink::TrackSourceSlot::COLLECTION) {
                return;
            }

            auto announcement = beatlink::DeviceFinder::getInstance().getLatestAnnouncementFrom(slot.getPlayer());
            const bool useSqlite = announcement && announcement->isUsingDeviceLibraryPlus();
            const auto target = localDatabaseFile(slot, useSqlite ? SQLITE_FILENAME : DEVICE_SQL_FILENAME);
            if (!std::filesystem::exists(target)) {
                return;
            }

            if (useSqlite) {
                auto connection = std::make_shared<SQLiteConnection>(target.string());
                {
                    std::lock_guard<std::mutex> lock(sqliteMutex_);
                    if (sqliteConnections_.contains(slot)) {
                        return;
                    }
                    sqliteConnections_[slot] = connection;
                }
                deliverConnectionUpdate(slot, connection, true);
            } else {
                auto database = std::make_shared<Database>(target.string());
                {
                    std::lock_guard<std::mutex> lock(databasesMutex_);
                    if (databases_.contains(slot)) {
                        return;
                    }
                    databases_[slot] = database;
                }
                deliverDatabaseUpdate(slot, database, true);
            }
        });

    deviceAnnouncementListener_ = std::make_shared<beatlink::DeviceAnnouncementCallbacks>(
        [](const beatlink::DeviceAnnouncement&) {},
        [this](const beatlink::DeviceAnnouncement& announcement) {
            const int player = announcement.getDeviceNumber();
            std::vector<SlotReference> slotsToRemove;
            {
                std::lock_guard<std::mutex> lock(databasesMutex_);
                for (const auto& [slot, database] : databases_) {
                    if (slot.getPlayer() == player) {
                        slotsToRemove.push_back(slot);
                    }
                }
            }
            for (const auto& slot : slotsToRemove) {
                std::shared_ptr<Database> database;
                {
                    std::lock_guard<std::mutex> lock(databasesMutex_);
                    auto it = databases_.find(slot);
                    if (it != databases_.end()) {
                        database = it->second;
                        databases_.erase(it);
                    }
                }
                if (database) {
                    deliverDatabaseUpdate(slot, database, false);
                }
            }
        });
}

void CrateDigger::setRetryLimit(int limit) {
    if (limit < 1 || limit > 10) {
        throw std::invalid_argument("limit must be between 1 and 10");
    }
    retryLimit_.store(limit);
}

std::string CrateDigger::humanReadableByteCount(int64_t bytes, bool si) {
    const int unit = si ? 1000 : 1024;
    if (bytes < unit) {
        return std::to_string(bytes) + " B";
    }
    const int exp = static_cast<int>(std::log(static_cast<double>(bytes)) / std::log(static_cast<double>(unit)));
    const std::string prefixes = si ? "kMGTPE" : "KMGTPE";
    const char prefix = prefixes.at(static_cast<size_t>(exp - 1));
    const std::string suffix = si ? std::string(1, prefix) : (std::string(1, prefix) + "i");
    const double value = bytes / std::pow(unit, exp);
    return std::format("{:.1f} {}B", value, suffix);
}

bool CrateDigger::canUseDatabase(const SlotReference& slot) const {
    auto announcement = beatlink::DeviceFinder::getInstance().getLatestAnnouncementFrom(slot.getPlayer());
    if (!announcement) {
        return false;
    }
    if (!announcement->isUsingDeviceLibraryPlus()) {
        return true;
    }
    return OpusProvider::getInstance().usingDeviceLibraryPlus();
}

std::shared_ptr<Database> CrateDigger::findDatabase(const DataReference& reference) const {
    return findDatabase(reference.getSlotReference());
}

std::shared_ptr<Database> CrateDigger::findDatabase(const SlotReference& slot) const {
    std::lock_guard<std::mutex> lock(databasesMutex_);
    auto it = databases_.find(slot);
    return (it == databases_.end()) ? nullptr : it->second;
}

std::shared_ptr<SQLiteConnection> CrateDigger::findSQLiteConnection(const DataReference& reference) const {
    return findSQLiteConnection(reference.getSlotReference());
}

std::shared_ptr<SQLiteConnection> CrateDigger::findSQLiteConnection(const SlotReference& slot) const {
    std::lock_guard<std::mutex> lock(sqliteMutex_);
    auto it = sqliteConnections_.find(slot);
    return (it == sqliteConnections_.end()) ? nullptr : it->second;
}

void CrateDigger::start() {
    if (isRunning()) {
        return;
    }
    MetadataFinder::getInstance().start();
    MetadataFinder::getInstance().addMetadataProvider(metadataProvider_);
    MetadataFinder::getInstance().addMountListener(mountListener_);
    beatlink::VirtualCdj::getInstance().addMediaDetailsListener(mediaDetailsListener_);
    beatlink::DeviceFinder::getInstance().addDeviceAnnouncementListener(deviceAnnouncementListener_);

    running_.store(true);

    for (const auto& details : MetadataFinder::getInstance().getMountedMediaDetails()) {
        if (mediaDetailsListener_) {
            mediaDetailsListener_->detailsAvailable(details);
        }
    }
}

void CrateDigger::stop() {
    if (!isRunning()) {
        return;
    }
    running_.store(false);
    MetadataFinder::getInstance().removeMetadataProvider(metadataProvider_);
    MetadataFinder::getInstance().removeMountListener(mountListener_);
    beatlink::VirtualCdj::getInstance().removeMediaDetailsListener(mediaDetailsListener_);

    {
        std::lock_guard<std::mutex> lock(databasesMutex_);
        databases_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(sqliteMutex_);
        sqliteConnections_.clear();
    }
}

void CrateDigger::addDatabaseListener(const DatabaseListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(dbListenersMutex_);
    dbListeners_.push_back(listener);
}

void CrateDigger::removeDatabaseListener(const DatabaseListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(dbListenersMutex_);
    dbListeners_.erase(std::remove(dbListeners_.begin(), dbListeners_.end(), listener), dbListeners_.end());
}

std::vector<DatabaseListenerPtr> CrateDigger::getDatabaseListeners() const {
    std::lock_guard<std::mutex> lock(dbListenersMutex_);
    return dbListeners_;
}

void CrateDigger::addSQLiteListener(const SQLiteConnectionListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(sqliteListenersMutex_);
    sqliteListeners_.push_back(listener);
}

void CrateDigger::removeSQLiteListener(const SQLiteConnectionListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(sqliteListenersMutex_);
    sqliteListeners_.erase(std::remove(sqliteListeners_.begin(), sqliteListeners_.end(), listener),
                           sqliteListeners_.end());
}

std::vector<SQLiteConnectionListenerPtr> CrateDigger::getSQLiteListeners() const {
    std::lock_guard<std::mutex> lock(sqliteListenersMutex_);
    return sqliteListeners_;
}

std::filesystem::path CrateDigger::createDownloadDirectory() const {
    const auto baseDir = std::filesystem::temp_directory_path();
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const std::string baseName = "bl-" + std::to_string(stamp) + "-";
    for (int attempt = 0; attempt < 1000; ++attempt) {
        auto candidate = baseDir / (baseName + std::to_string(attempt));
        if (std::filesystem::create_directory(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Failed to create download directory for CrateDigger");
}

std::filesystem::path CrateDigger::localDatabaseFile(const SlotReference& slot, const std::string& name) const {
    return downloadDirectory_ / (slotPrefix(slot) + name);
}

std::string CrateDigger::slotPrefix(const SlotReference& slot) const {
    return "player-" + std::to_string(slot.getPlayer()) +
           "-slot-" + std::to_string(static_cast<int>(slot.getSlot())) + "-";
}

void CrateDigger::deliverDatabaseUpdate(const SlotReference& slot,
                                        const std::shared_ptr<Database>& database,
                                        bool available) {
    auto listeners = getDatabaseListeners();
    for (const auto& listener : listeners) {
        if (!listener || !database) {
            continue;
        }
        try {
            if (available) {
                listener->databaseMounted(slot, *database);
            } else {
                listener->databaseUnmounted(slot, *database);
            }
        } catch (...) {
        }
    }
}

void CrateDigger::deliverConnectionUpdate(const SlotReference& slot,
                                          const std::shared_ptr<SQLiteConnection>& connection,
                                          bool available) {
    auto listeners = getSQLiteListeners();
    for (const auto& listener : listeners) {
        if (!listener || !connection) {
            continue;
        }
        try {
            if (available) {
                listener->databaseConnected(slot, *connection);
            } else {
                listener->databaseDisconnected(slot, *connection);
            }
        } catch (...) {
        }
    }
}

} // namespace beatlink::data
