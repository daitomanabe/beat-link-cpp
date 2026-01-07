#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BeatGrid.hpp"
#include "DataReference.hpp"
#include "DeckReference.hpp"
#include "MetadataProvider.hpp"
#include "MountListener.hpp"
#include "SlotReference.hpp"
#include "TrackMetadata.hpp"
#include "TrackMetadataListener.hpp"
#include "TrackMetadataUpdate.hpp"
#include "beatlink/CdjStatus.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/DeviceUpdateListener.hpp"
#include "beatlink/LifecycleParticipant.hpp"
#include "beatlink/MediaDetails.hpp"
#include "beatlink/dbserver/ConnectionManager.hpp"

namespace beatlink::data {

class MetadataFinder : public beatlink::LifecycleParticipant {
public:
    static constexpr int MENU_TIMEOUT_SECONDS = 20;

    static MetadataFinder& getInstance();

    bool isRunning() const override { return running_.load(); }

    bool isPassive() const { return passive_.load(); }
    void setPassive(bool passive) { passive_.store(passive); }

    std::shared_ptr<TrackMetadata> requestMetadataFrom(const beatlink::CdjStatus& status);
    std::shared_ptr<TrackMetadata> requestMetadataFrom(const DataReference& track);
    std::shared_ptr<TrackMetadata> requestMetadataFrom(const DataReference& track, beatlink::TrackType trackType);

    std::unordered_map<DeckReference, std::shared_ptr<TrackMetadata>, DeckReferenceHash> getLoadedTracks();
    std::shared_ptr<TrackMetadata> getLatestMetadataFor(int player);
    std::shared_ptr<TrackMetadata> getLatestMetadataFor(const beatlink::DeviceUpdate& update);

    std::unordered_set<SlotReference, SlotReferenceHash> getMountedMediaSlots() const;
    std::vector<beatlink::MediaDetails> getMountedMediaDetails() const;
    std::optional<beatlink::MediaDetails> getMediaDetailsFor(const SlotReference& slot) const;

    void addMountListener(const MountListenerPtr& listener);
    void removeMountListener(const MountListenerPtr& listener);
    std::vector<MountListenerPtr> getMountListeners() const;

    void addTrackMetadataListener(const TrackMetadataListenerPtr& listener);
    void removeTrackMetadataListener(const TrackMetadataListenerPtr& listener);
    std::vector<TrackMetadataListenerPtr> getTrackMetadataListeners() const;

    void addMetadataProvider(const MetadataProviderPtr& provider);
    void removeMetadataProvider(const MetadataProviderPtr& provider);
    std::vector<MetadataProviderPtr> getMetadataProviders(const beatlink::MediaDetails* sourceMedia) const;
    MetadataProvider& getCompositeProvider();

    void start();
    void stop();

    std::vector<beatlink::dbserver::Message> requestPlaylistItemsFrom(int player,
                                                                      beatlink::TrackSourceSlot slot,
                                                                      int sortOrder,
                                                                      int playlistOrFolderId,
                                                                      bool folder);

private:
    MetadataFinder();

    class CompositeProvider final : public MetadataProvider {
    public:
        explicit CompositeProvider(MetadataFinder& owner)
            : owner_(owner)
        {
        }

        std::vector<beatlink::MediaDetails> supportedMedia() const override { return {}; }

        std::shared_ptr<TrackMetadata> getTrackMetadata(const beatlink::MediaDetails& sourceMedia,
                                                        const DataReference& track) override;
        std::shared_ptr<AlbumArt> getAlbumArt(const beatlink::MediaDetails& sourceMedia,
                                              const DataReference& art) override;
        std::shared_ptr<BeatGrid> getBeatGrid(const beatlink::MediaDetails& sourceMedia,
                                              const DataReference& track) override;
        std::shared_ptr<CueList> getCueList(const beatlink::MediaDetails& sourceMedia,
                                            const DataReference& track) override;
        std::shared_ptr<WaveformPreview> getWaveformPreview(const beatlink::MediaDetails& sourceMedia,
                                                            const DataReference& track) override;
        std::shared_ptr<WaveformDetail> getWaveformDetail(const beatlink::MediaDetails& sourceMedia,
                                                          const DataReference& track) override;
        std::shared_ptr<AnalysisTag> getAnalysisSection(const beatlink::MediaDetails& sourceMedia,
                                                        const DataReference& track,
                                                        const std::string& fileExtension,
                                                        const std::string& typeTag) override;

    private:
        MetadataFinder& owner_;
    };

    std::shared_ptr<TrackMetadata> requestMetadataInternal(const DataReference& track, bool failIfPassive);

    std::shared_ptr<TrackMetadata> queryMetadata(const DataReference& track, beatlink::dbserver::Client& client);
    std::shared_ptr<CueList> getCueList(int rekordboxId, beatlink::TrackSourceSlot slot, beatlink::dbserver::Client& client);
    std::vector<beatlink::dbserver::Message> getFullTrackList(beatlink::TrackSourceSlot slot,
                                                              beatlink::dbserver::Client& client,
                                                              int sortOrder);
    std::vector<beatlink::dbserver::Message> getPlaylistItems(beatlink::TrackSourceSlot slot,
                                                              int sortOrder,
                                                              int playlistOrFolderId,
                                                              bool folder,
                                                              beatlink::dbserver::Client& client);

    void clearDeck(const beatlink::CdjStatus& update);
    void clearMetadata(const beatlink::DeviceAnnouncement& announcement);
    void updateMetadata(const beatlink::CdjStatus& update, const std::shared_ptr<TrackMetadata>& data);

    void flushHotCacheSlot(const SlotReference& slot);
    void recordMount(const SlotReference& slot);
    void removeMount(const SlotReference& slot);

    void deliverMountUpdate(const SlotReference& slot, bool mounted);
    void deliverTrackMetadataUpdate(int player, const std::shared_ptr<TrackMetadata>& metadata);

    void handleUpdate(const beatlink::CdjStatus& update);

    void enqueueUpdate(const beatlink::CdjStatus& update);

    std::atomic<bool> running_{false};
    std::atomic<bool> passive_{false};

    mutable std::mutex hotCacheMutex_;
    std::unordered_map<DeckReference, std::shared_ptr<TrackMetadata>, DeckReferenceHash> hotCache_;

    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
    std::deque<beatlink::CdjStatus> pendingUpdates_;
    std::thread queueHandler_;

    mutable std::mutex mediaMountsMutex_;
    std::unordered_set<SlotReference, SlotReferenceHash> mediaMounts_;

    mutable std::mutex mediaDetailsMutex_;
    std::unordered_map<SlotReference, beatlink::MediaDetails, SlotReferenceHash> mediaDetails_;

    mutable std::mutex mountListenersMutex_;
    std::vector<MountListenerPtr> mountListeners_;

    mutable std::mutex trackListenersMutex_;
    std::vector<TrackMetadataListenerPtr> trackListeners_;

    mutable std::mutex metadataProvidersMutex_;
    std::unordered_map<std::string, std::vector<MetadataProviderPtr>> metadataProviders_;

    mutable std::mutex activeRequestsMutex_;
    std::unordered_set<int> activeRequests_;

    beatlink::DeviceUpdateListenerPtr updateListener_;
    beatlink::DeviceAnnouncementListenerPtr announcementListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;
    beatlink::MediaDetailsListenerPtr mediaDetailsListener_;

    CompositeProvider compositeProvider_{*this};
};

} // namespace beatlink::data
