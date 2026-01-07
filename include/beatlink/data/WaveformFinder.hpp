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

#include "DataReference.hpp"
#include "DeckReference.hpp"
#include "MetadataFinder.hpp"
#include "MountListener.hpp"
#include "TrackMetadataListener.hpp"
#include "TrackMetadataUpdate.hpp"
#include "WaveformDetail.hpp"
#include "WaveformListener.hpp"
#include "WaveformPreview.hpp"
#include "WaveformTypes.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/LifecycleParticipant.hpp"

namespace beatlink::data {

class WaveformFinder : public beatlink::LifecycleParticipant {
public:
    static constexpr int64_t MAXIMUM_ANALYSIS_WAIT_NANOS = 90LL * 1000000000LL;
    static constexpr int64_t ANALYSIS_UPDATE_INTERVAL_MS = 10000;

    static WaveformFinder& getInstance();

    bool isRunning() const override { return running_.load(); }

    void setFindDetails(bool findDetails);
    bool isFindingDetails() const { return findDetails_.load(); }

    void setPreferredStyle(WaveformStyle style);
    WaveformStyle getPreferredStyle() const { return preferredStyle_.load(); }

    void setColorPreferred(bool preferColor);
    bool isColorPreferred() const { return preferredStyle_.load() != WaveformStyle::BLUE; }

    std::unordered_map<DeckReference, std::shared_ptr<WaveformPreview>, DeckReferenceHash> getLoadedPreviews();
    std::unordered_map<DeckReference, std::shared_ptr<WaveformDetail>, DeckReferenceHash> getLoadedDetails();

    std::shared_ptr<WaveformPreview> getLatestPreviewFor(int player);
    std::shared_ptr<WaveformPreview> getLatestPreviewFor(const beatlink::DeviceUpdate& update);

    std::shared_ptr<WaveformDetail> getLatestDetailFor(int player);
    std::shared_ptr<WaveformDetail> getLatestDetailFor(const beatlink::DeviceUpdate& update);

    std::shared_ptr<WaveformPreview> requestWaveformPreviewFrom(const DataReference& dataReference);
    std::shared_ptr<WaveformDetail> requestWaveformDetailFrom(const DataReference& dataReference);

    void addWaveformListener(const WaveformListenerPtr& listener);
    void removeWaveformListener(const WaveformListenerPtr& listener);
    std::vector<WaveformListenerPtr> getWaveformListeners() const;

    void start();
    void stop();

    static bool retryUnanalyzedTrack(const TrackMetadataUpdate* fromUpdate,
                                     std::atomic<bool>& retryFlag,
                                     const TrackMetadataListenerPtr& retryListener,
                                     const std::string& description);

private:
    WaveformFinder();

    std::shared_ptr<WaveformPreview> requestPreviewInternal(const DataReference& trackReference,
                                                            const TrackMetadataUpdate* fromUpdate);
    std::shared_ptr<WaveformDetail> requestDetailInternal(const DataReference& trackReference,
                                                          const TrackMetadataUpdate* fromUpdate);

    std::shared_ptr<WaveformPreview> getWaveformPreview(int rekordboxId,
                                                        const SlotReference& slot,
                                                        beatlink::TrackType trackType,
                                                        const TrackMetadataUpdate* fromUpdate,
                                                        beatlink::dbserver::Client& client);
    std::shared_ptr<WaveformDetail> getWaveformDetail(int rekordboxId,
                                                      const SlotReference& slot,
                                                      beatlink::TrackType trackType,
                                                      const TrackMetadataUpdate* fromUpdate,
                                                      beatlink::dbserver::Client& client);

    void clearDeckPreview(const TrackMetadataUpdate& update);
    void clearDeckDetail(const TrackMetadataUpdate& update);
    void clearDeck(const TrackMetadataUpdate& update);
    void clearWaveforms(const beatlink::DeviceAnnouncement& announcement);

    void updatePreview(const TrackMetadataUpdate& update, const std::shared_ptr<WaveformPreview>& preview);
    void updateDetail(const TrackMetadataUpdate& update, const std::shared_ptr<WaveformDetail>& detail);

    void deliverWaveformPreviewUpdate(int player, const std::shared_ptr<WaveformPreview>& preview);
    void deliverWaveformDetailUpdate(int player, const std::shared_ptr<WaveformDetail>& detail);

    void handleUpdate(const TrackMetadataUpdate& update);
    void enqueueUpdate(const TrackMetadataUpdate& update);

    void primeCache();
    void clearAllWaveforms();

    std::atomic<bool> running_{false};
    std::atomic<bool> retrying_{false};
    std::atomic<bool> findDetails_{true};
    std::atomic<WaveformStyle> preferredStyle_{WaveformStyle::RGB};

    mutable std::mutex previewCacheMutex_;
    std::unordered_map<DeckReference, std::shared_ptr<WaveformPreview>, DeckReferenceHash> previewHotCache_;

    mutable std::mutex detailCacheMutex_;
    std::unordered_map<DeckReference, std::shared_ptr<WaveformDetail>, DeckReferenceHash> detailHotCache_;

    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
    std::deque<TrackMetadataUpdate> pendingUpdates_;
    std::thread queueHandler_;

    mutable std::mutex activePreviewMutex_;
    std::unordered_set<int> activePreviewRequests_;

    mutable std::mutex activeDetailMutex_;
    std::unordered_set<int> activeDetailRequests_;

    mutable std::mutex listenersMutex_;
    std::vector<WaveformListenerPtr> waveformListeners_;

    TrackMetadataListenerPtr metadataListener_;
    MountListenerPtr mountListener_;
    beatlink::DeviceAnnouncementListenerPtr announcementListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;
};

} // namespace beatlink::data
