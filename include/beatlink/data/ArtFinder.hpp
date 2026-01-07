#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AlbumArt.hpp"
#include "AlbumArtListener.hpp"
#include "DataReference.hpp"
#include "DeckReference.hpp"
#include "MetadataFinder.hpp"
#include "MountListener.hpp"
#include "TrackMetadataListener.hpp"
#include "TrackMetadataUpdate.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/LifecycleParticipant.hpp"

namespace beatlink::data {

class ArtFinder : public beatlink::LifecycleParticipant {
public:
    static constexpr int DEFAULT_ART_CACHE_SIZE = 100;

    static ArtFinder& getInstance();

    bool isRunning() const override { return running_.load(); }

    int getArtCacheSize() const { return artCacheSize_.load(); }
    void setArtCacheSize(int size);

    bool getRequestHighResolutionArt() const { return requestHighResolutionArt_.load(); }
    void setRequestHighResolutionArt(bool shouldRequest) { requestHighResolutionArt_.store(shouldRequest); }

    std::unordered_map<DeckReference, std::shared_ptr<AlbumArt>, DeckReferenceHash> getLoadedArt();
    std::shared_ptr<AlbumArt> getLatestArtFor(int player);
    std::shared_ptr<AlbumArt> getLatestArtFor(const beatlink::DeviceUpdate& update);

    std::shared_ptr<AlbumArt> requestArtworkFrom(const DataReference& artReference);
    std::shared_ptr<AlbumArt> requestArtworkFrom(const DataReference& artReference, beatlink::TrackType trackType);

    void addAlbumArtListener(const AlbumArtListenerPtr& listener);
    void removeAlbumArtListener(const AlbumArtListenerPtr& listener);
    std::vector<AlbumArtListenerPtr> getAlbumArtListeners() const;

    void start();
    void stop();

private:
    ArtFinder();

    std::shared_ptr<AlbumArt> requestArtworkInternal(const DataReference& artReference, bool failIfPassive);
    std::shared_ptr<AlbumArt> getArtwork(int artworkId,
                                         const SlotReference& slot,
                                         beatlink::TrackType trackType,
                                         beatlink::dbserver::Client& client);

    std::shared_ptr<AlbumArt> findArtInMemoryCaches(const DataReference& artReference);

    void clearDeck(const TrackMetadataUpdate& update);
    void clearArt(const beatlink::DeviceAnnouncement& announcement);
    void updateArt(const TrackMetadataUpdate& update, const std::shared_ptr<AlbumArt>& art);

    void deliverAlbumArtUpdate(int player, const std::shared_ptr<AlbumArt>& art);

    void handleUpdate(const TrackMetadataUpdate& update);
    void enqueueUpdate(const TrackMetadataUpdate& update);

    void removeArtFromCache(const DataReference& artReference);
    void addArtToCache(const DataReference& artReference, const std::shared_ptr<AlbumArt>& art);
    void evictFromArtCache();

    std::atomic<bool> running_{false};

    mutable std::mutex hotCacheMutex_;
    std::unordered_map<DeckReference, std::shared_ptr<AlbumArt>, DeckReferenceHash> hotCache_;

    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
    std::deque<TrackMetadataUpdate> pendingUpdates_;
    std::thread queueHandler_;

    mutable std::mutex activeRequestsMutex_;
    std::unordered_set<int> activeRequests_;

    mutable std::mutex artCacheMutex_;
    std::unordered_map<DataReference, std::shared_ptr<AlbumArt>, DataReferenceHash> artCache_;
    std::deque<DataReference> artCacheEvictionQueue_;
    std::unordered_set<DataReference, DataReferenceHash> artCacheRecentlyUsed_;

    std::atomic<int> artCacheSize_{DEFAULT_ART_CACHE_SIZE};
    std::atomic<bool> requestHighResolutionArt_{true};

    mutable std::mutex listenersMutex_;
    std::vector<AlbumArtListenerPtr> artListeners_;

    TrackMetadataListenerPtr metadataListener_;
    MountListenerPtr mountListener_;
    beatlink::DeviceAnnouncementListenerPtr announcementListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;
};

} // namespace beatlink::data
