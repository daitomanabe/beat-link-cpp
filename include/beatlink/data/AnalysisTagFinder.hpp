#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AnalysisTag.hpp"
#include "AnalysisTagListener.hpp"
#include "DeckReference.hpp"
#include "MetadataFinder.hpp"
#include "MountListener.hpp"
#include "TrackMetadataListener.hpp"
#include "TrackMetadataUpdate.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/LifecycleParticipant.hpp"
#include "beatlink/dbserver/Client.hpp"
#include "beatlink/dbserver/ConnectionManager.hpp"
#include "beatlink/dbserver/NumberField.hpp"

namespace beatlink::data {

class AnalysisTagFinder : public beatlink::LifecycleParticipant {
public:
    struct CacheEntry {
        DataReference dataReference;
        std::string fileExtension;
        std::string typeTag;
        std::shared_ptr<AnalysisTag> analysisTag;

        CacheEntry(DataReference reference,
                   std::string fileExtension,
                   std::string typeTag,
                   std::shared_ptr<AnalysisTag> tag)
            : dataReference(std::move(reference))
            , fileExtension(std::move(fileExtension))
            , typeTag(std::move(typeTag))
            , analysisTag(std::move(tag))
        {
        }
    };

    static AnalysisTagFinder& getInstance();

    bool isRunning() const override { return running_.load(); }

    std::unordered_map<DeckReference, std::unordered_map<std::string, CacheEntry>, DeckReferenceHash>
    getLoadedAnalysisTags();

    std::shared_ptr<AnalysisTag> getLatestTrackAnalysisFor(int player,
                                                           const std::string& fileExtension,
                                                           const std::string& typeTag);
    std::shared_ptr<AnalysisTag> getLatestTrackAnalysisFor(const beatlink::DeviceUpdate& update,
                                                           const std::string& fileExtension,
                                                           const std::string& typeTag);

    std::shared_ptr<AnalysisTag> requestAnalysisTagFrom(const DataReference& trackReference,
                                                        const std::string& fileExtension,
                                                        const std::string& typeTag);

    beatlink::dbserver::NumberField stringToProtocolNumber(const std::string& value);

    void addAnalysisTagListener(const AnalysisTagListenerPtr& listener,
                                const std::string& fileExtension,
                                const std::string& typeTag);
    void removeAnalysisTagListener(const AnalysisTagListenerPtr& listener,
                                   const std::string& fileExtension,
                                   const std::string& typeTag);

    std::unordered_map<std::string, std::vector<AnalysisTagListenerPtr>> getTagListeners() const;

    void start();
    void stop();

private:
    AnalysisTagFinder();

    std::shared_ptr<AnalysisTag> requestAnalysisTagInternal(const DataReference& trackReference,
                                                            const std::string& fileExtension,
                                                            const std::string& typeTag,
                                                            bool failIfPassive);

    std::shared_ptr<AnalysisTag> getTagViaDbServer(const DataReference& trackReference,
                                                   const std::string& fileExtension,
                                                   const std::string& typeTag,
                                                   beatlink::dbserver::Client& client);

    void enqueueUpdate(const TrackMetadataUpdate& update);
    void handleUpdate(const TrackMetadataUpdate& update);

    void clearDeckTags(const TrackMetadataUpdate& update);
    void clearTags(const beatlink::DeviceAnnouncement& announcement);

    void updateAnalysisTag(const TrackMetadataUpdate& update,
                           const std::string& fileExtension,
                           const std::string& typeTag,
                           const std::shared_ptr<AnalysisTag>& tag);

    void deliverAnalysisTagUpdate(int player,
                                  const std::string& fileExtension,
                                  const std::string& typeTag,
                                  const std::shared_ptr<AnalysisTag>& tag);
    void deliverTagLossUpdate(int player,
                              const std::unordered_map<std::string, CacheEntry>& tags);

    void primeCache();

    std::atomic<bool> running_{false};

    mutable std::mutex hotCacheMutex_;
    std::unordered_map<DeckReference, std::unordered_map<std::string, CacheEntry>, DeckReferenceHash> hotCache_;

    mutable std::mutex pendingMutex_;
    std::condition_variable pendingCv_;
    std::deque<TrackMetadataUpdate> pendingUpdates_;
    std::thread queueHandler_;

    mutable std::mutex listenersMutex_;
    std::unordered_map<std::string, std::vector<AnalysisTagListenerPtr>> analysisTagListeners_;

    mutable std::mutex activeRequestsMutex_;
    std::unordered_set<std::string> activeRequests_;

    TrackMetadataListenerPtr metadataListener_;
    MountListenerPtr mountListener_;
    beatlink::DeviceAnnouncementListenerPtr announcementListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;
};

} // namespace beatlink::data
