#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "BeatGridFinder.hpp"
#include "TrackPositionBeatListener.hpp"
#include "TrackPositionListener.hpp"
#include "TrackPositionUpdate.hpp"
#include "beatlink/BeatFinder.hpp"
#include "beatlink/DeviceAnnouncementListener.hpp"
#include "beatlink/DeviceUpdateListener.hpp"
#include "beatlink/LifecycleParticipant.hpp"
#include "beatlink/PrecisePositionListener.hpp"
#include "beatlink/VirtualCdj.hpp"

namespace beatlink::data {

class TimeFinder : public beatlink::LifecycleParticipant {
public:
    static TimeFinder& getInstance();

    bool isRunning() const override { return running_.load(); }

    std::unordered_map<int, std::shared_ptr<TrackPositionUpdate>> getLatestPositions();
    std::unordered_map<int, std::shared_ptr<beatlink::DeviceUpdate>> getLatestUpdates();

    std::shared_ptr<TrackPositionUpdate> getLatestPositionFor(int player);
    std::shared_ptr<TrackPositionUpdate> getLatestPositionFor(const beatlink::DeviceUpdate& update);

    std::shared_ptr<beatlink::DeviceUpdate> getLatestUpdateFor(int player);

    int64_t getTimeFor(int player);
    int64_t getTimeFor(const beatlink::DeviceUpdate& update);

    void addTrackPositionListener(int player, const TrackPositionListenerPtr& listener);
    void removeTrackPositionListener(const TrackPositionListenerPtr& listener);

    int64_t getSlack() const;
    void setSlack(int64_t slack);

    void setUsePrecisePositionPackets(bool use);
    bool isUsingPrecisePositionPackets() const;

    bool isOwnBeatListener(const beatlink::BeatListenerPtr& listener) const;

    void start();
    void stop();

private:
    TimeFinder();

    struct ListenerEntry {
        TrackPositionListenerPtr listener;
        int player{0};
        std::shared_ptr<TrackPositionUpdate> lastUpdate;
        bool lastUpdateWasNull{true};
    };

    int64_t interpolateTimeSinceUpdate(const TrackPositionUpdate& update, int64_t currentTimestamp) const;
    std::optional<CueList::Entry> findAdjacentCue(const beatlink::CdjStatus& update,
                                                  const std::shared_ptr<BeatGrid>& beatGrid) const;
    int64_t interpolateTimeFromUpdate(const TrackPositionUpdate& lastUpdate,
                                      const beatlink::CdjStatus& update,
                                      const std::shared_ptr<BeatGrid>& beatGrid) const;
    int64_t timeOfBeat(const BeatGrid& beatGrid, int beatNumber, const beatlink::DeviceUpdate& update) const;

    void updateListenersIfNeeded(int player,
                                 const std::shared_ptr<TrackPositionUpdate>& update,
                                 const beatlink::Beat* beat);

    bool interpolationsDisagree(const TrackPositionUpdate& lastUpdate,
                                const TrackPositionUpdate& currentUpdate) const;
    bool pitchesDiffer(const TrackPositionUpdate& lastUpdate,
                       const TrackPositionUpdate& currentUpdate) const;

    std::atomic<bool> running_{false};
    std::atomic<int64_t> slack_{50};
    std::atomic<bool> usePrecisePositionPackets_{true};

    mutable std::mutex positionsMutex_;
    std::unordered_map<int, std::shared_ptr<TrackPositionUpdate>> positions_;

    mutable std::mutex updatesMutex_;
    std::unordered_map<int, std::shared_ptr<beatlink::DeviceUpdate>> updates_;

    mutable std::mutex listenersMutex_;
    std::vector<ListenerEntry> trackPositionListeners_;

    beatlink::DeviceAnnouncementListenerPtr announcementListener_;
    beatlink::DeviceUpdateListenerPtr updateListener_;
    beatlink::PrecisePositionListenerPtr positionListener_;
    beatlink::LifecycleListenerPtr lifecycleListener_;
    beatlink::BeatListenerPtr beatListener_;
};

} // namespace beatlink::data
