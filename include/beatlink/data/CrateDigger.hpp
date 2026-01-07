#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Database.hpp"
#include "DatabaseListener.hpp"
#include "MetadataProvider.hpp"
#include "MountListener.hpp"
#include "SQLiteConnection.hpp"
#include "SQLiteConnectionListener.hpp"
#include "SlotReference.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/MediaDetailsListener.hpp"

namespace beatlink::data {

class CrateDigger {
public:
    static constexpr const char* DEVICE_SQL_FILENAME = "export.pdb";
    static constexpr const char* SQLITE_FILENAME = "exportLibrary.db";

    static CrateDigger& getInstance();

    int getRetryLimit() const { return retryLimit_.load(); }
    void setRetryLimit(int limit);

    bool isRunning() const { return running_.load(); }

    static std::string humanReadableByteCount(int64_t bytes, bool si);

    bool canUseDatabase(const SlotReference& slot) const;

    std::shared_ptr<Database> findDatabase(const DataReference& reference) const;
    std::shared_ptr<Database> findDatabase(const SlotReference& slot) const;

    std::shared_ptr<SQLiteConnection> findSQLiteConnection(const DataReference& reference) const;
    std::shared_ptr<SQLiteConnection> findSQLiteConnection(const SlotReference& slot) const;

    void start();
    void stop();

    void addDatabaseListener(const DatabaseListenerPtr& listener);
    void removeDatabaseListener(const DatabaseListenerPtr& listener);
    std::vector<DatabaseListenerPtr> getDatabaseListeners() const;

    void addSQLiteListener(const SQLiteConnectionListenerPtr& listener);
    void removeSQLiteListener(const SQLiteConnectionListenerPtr& listener);
    std::vector<SQLiteConnectionListenerPtr> getSQLiteListeners() const;

    const std::filesystem::path& getDownloadDirectory() const { return downloadDirectory_; }

private:
    CrateDigger();

    std::filesystem::path createDownloadDirectory() const;
    std::filesystem::path localDatabaseFile(const SlotReference& slot, const std::string& name) const;
    std::string slotPrefix(const SlotReference& slot) const;

    void deliverDatabaseUpdate(const SlotReference& slot,
                               const std::shared_ptr<Database>& database,
                               bool available);
    void deliverConnectionUpdate(const SlotReference& slot,
                                 const std::shared_ptr<SQLiteConnection>& connection,
                                 bool available);

    std::atomic<int> retryLimit_{3};
    std::atomic<bool> running_{false};

    std::filesystem::path downloadDirectory_;

    mutable std::mutex databasesMutex_;
    std::unordered_map<SlotReference, std::shared_ptr<Database>, SlotReferenceHash> databases_;

    mutable std::mutex sqliteMutex_;
    std::unordered_map<SlotReference, std::shared_ptr<SQLiteConnection>, SlotReferenceHash> sqliteConnections_;

    mutable std::mutex dbListenersMutex_;
    std::vector<DatabaseListenerPtr> dbListeners_;

    mutable std::mutex sqliteListenersMutex_;
    std::vector<SQLiteConnectionListenerPtr> sqliteListeners_;

    MetadataProviderPtr metadataProvider_;
    MountListenerPtr mountListener_;
    beatlink::MediaDetailsListenerPtr mediaDetailsListener_;
    beatlink::DeviceAnnouncementListenerPtr deviceAnnouncementListener_;
};

} // namespace beatlink::data
