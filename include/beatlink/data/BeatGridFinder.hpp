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

#include "BeatGrid.hpp"
#include "BeatGridListener.hpp"
#include "DataReference.hpp"
#include "DeckReference.hpp"
#include "MetadataFinder.hpp"
#include "MountListener.hpp"
#include "TrackMetadataListener.hpp"
#include "TrackMetadataUpdate.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/LifecycleParticipant.hpp"

namespace beatlink::data {

class BeatGridFinder : public beatlink::LifecycleParticipant {
public:
    static BeatGridFinder& getInstance();

    bool isRunning() const override { return running_.load(); }

    std::unordered_map<DeckReference, std::shared_ptr<BeatGrid>, DeckReferenceHash> getLoadedBeatGrids();
    std::shared_ptr<BeatGrid> getLatestBeatGridFor(int player);
    std::shared_ptr<BeatGrid> getLatestBeatGridFor(const beatlink::DeviceUpdate& update);

    std::shared_ptr<BeatGrid> requestBeatGridFrom(const DataReference& track);

    void addBeatGridListener(const BeatGridListenerPtr& listener);
    void removeBeatGridListener(const BeatGridListenerPtr& listener);
    std::vector<BeatGridListenerPtr> getBeatGridListeners() const;

    void start();
    void stop();

private:
    BeatGridFinder();

    std::shared_ptr<BeatGrid> requestBeatGridInternal(const DataReference& trackReference,
                                                      const TrackMetadataUpdate* fromUpdate);
    std::shared_ptr<BeatGrid> getBeatGrid(int rekordboxId,
                                          const SlotReference& slot,
                                          beatlink::TrackType trackType,
                                          const TrackMetadataUpdate* fromUpdate,
                                          beatlink::dbserver::Client& client);

    void clearDeck(const TrackMetadataUpdate& update);
    void clearBeatGrids(const beatlink::DeviceAnnouncement& announcement);
    void updateBeatGrid(const TrackMetadataUpdate& update, const std::shared_ptr<BeatGrid>& beatGrid);

    void deliverBeatGridUpdate(int player, const std::shared_ptr<BeatGrid>& beatGrid);

    void handleUpdate(const TrackMetadataUpdate& update);
    void enqueueUpdate(const TrackMetadataUpdate& update);

    std::atomic<bool> running_{false};
    std::atomic<bool> retrying_{false};

    mutable std::mutex hotCacheMutex_;
    std::unordered_map<DeckReference, std::shared_ptr<BeatGrid>, DeckReferenceHash> hotCache_;

    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
    std::deque<TrackMetadataUpdate> pendingUpdates_;
    std::thread queueHandler_;

    mutable std::mutex activeRequestsMutex_;
    std::unordered_set<int> activeRequests_;

    mutable std::mutex listenersMutex_;
    std::vector<BeatGridListenerPtr> beatGridListeners_;

    TrackMetadataListenerPtr metadataListener_;
    MountListenerPtr mountListener_;
    beatlink::DeviceAnnouncementListenerPtr announcementListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;
};

} // namespace beatlink::data
