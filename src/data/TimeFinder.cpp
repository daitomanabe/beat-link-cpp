#include "beatlink/data/TimeFinder.hpp"

#include "beatlink/DeviceFinder.hpp"
#include "beatlink/Util.hpp"
#include "beatlink/data/MetadataFinder.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <optional>

namespace beatlink::data {

namespace {
int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

TimeFinder& TimeFinder::getInstance() {
    static TimeFinder instance;
    return instance;
}

TimeFinder::TimeFinder() {
    announcementListener_ = std::make_shared<beatlink::DeviceAnnouncementCallbacks>(
        [](const beatlink::DeviceAnnouncement&) {},
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            std::lock_guard<std::mutex> lockPositions(positionsMutex_);
            std::lock_guard<std::mutex> lockUpdates(updatesMutex_);
            positions_.erase(announcement.getDeviceNumber());
            updates_.erase(announcement.getDeviceNumber());
        });

    updateListener_ = std::make_shared<beatlink::DeviceUpdateCallbacks>(
        [this](const beatlink::DeviceUpdate& update) {
            const auto* status = dynamic_cast<const beatlink::CdjStatus*>(&update);
            if (!status) {
                return;
            }
            const int device = update.getDeviceNumber();

            std::shared_ptr<TrackPositionUpdate> lastPosition;
            {
                std::lock_guard<std::mutex> lock(positionsMutex_);
                auto it = positions_.find(device);
                if (it != positions_.end()) {
                    lastPosition = it->second;
                }
            }

            if (lastPosition && lastPosition->precise) {
                return;
            }

            auto updateCopy = std::make_shared<beatlink::CdjStatus>(*status);
            {
                std::lock_guard<std::mutex> lock(updatesMutex_);
                auto it = updates_.find(device);
                if (it == updates_.end() || !it->second ||
                    it->second->getTimestampNanos() < update.getTimestampNanos()) {
                    updates_[device] = updateCopy;
                }
            }

            auto beatGrid = BeatGridFinder::getInstance().getLatestBeatGridFor(update);
            const int beatNumber = status->getBeatNumber();
            if (!beatGrid || beatNumber < 0) {
                bool removed = false;
                {
                    std::lock_guard<std::mutex> lock(positionsMutex_);
                    removed = positions_.erase(device) > 0;
                }
                if (removed) {
                    updateListenersIfNeeded(device, nullptr, nullptr);
                }
                return;
            }

            const int64_t timestamp = update.getTimestampNanos();
            std::shared_ptr<TrackPositionUpdate> newPosition;
            if (!lastPosition || lastPosition->beatGrid != beatGrid) {
                int64_t timeGuess = timeOfBeat(*beatGrid, beatNumber, update);
                if (auto cue = findAdjacentCue(*status, beatGrid)) {
                    timeGuess = cue->cueTime;
                }
                newPosition = std::make_shared<TrackPositionUpdate>(
                    timestamp,
                    timeGuess,
                    beatNumber,
                    false,
                    status->isPlaying(),
                    beatlink::Util::pitchToMultiplier(update.getPitch()),
                    status->isPlayingBackwards(),
                    beatGrid,
                    false,
                    false);
            } else {
                const int64_t newTime = interpolateTimeFromUpdate(*lastPosition, *status, beatGrid);
                const bool newReverse = status->isPlayingBackwards();
                const bool newPlaying = status->isPlaying() && (!newReverse || newTime > 0);
                newPosition = std::make_shared<TrackPositionUpdate>(
                    timestamp,
                    newTime,
                    beatNumber,
                    false,
                    newPlaying,
                    beatlink::Util::pitchToMultiplier(update.getPitch()),
                    newReverse,
                    beatGrid,
                    false,
                    false);
            }

            bool shouldUpdate = true;
            {
                std::lock_guard<std::mutex> lock(positionsMutex_);
                auto it = positions_.find(device);
                if (it != positions_.end() && it->second &&
                    it->second->timestamp >= newPosition->timestamp) {
                    shouldUpdate = false;
                } else {
                    positions_[device] = newPosition;
                }
            }

            if (shouldUpdate) {
                updateListenersIfNeeded(device, newPosition, nullptr);
            }
        });

    beatListener_ = std::make_shared<beatlink::BeatListenerCallbacks>(
        [this](const beatlink::Beat& beat) {
            const int device = beat.getDeviceNumber();
            if (device >= 16) {
                return;
            }

            auto beatCopy = std::make_shared<beatlink::Beat>(beat);
            {
                std::lock_guard<std::mutex> lock(updatesMutex_);
                auto it = updates_.find(device);
                if (it == updates_.end() || !it->second ||
                    it->second->getTimestampNanos() < beat.getTimestampNanos()) {
                    updates_[device] = beatCopy;
                }
            }

            auto beatGrid = BeatGridFinder::getInstance().getLatestBeatGridFor(beat);
            std::shared_ptr<TrackPositionUpdate> lastPosition;
            {
                std::lock_guard<std::mutex> lock(positionsMutex_);
                auto it = positions_.find(device);
                if (it != positions_.end()) {
                    lastPosition = it->second;
                }
            }

            if (beatGrid) {
                if (!lastPosition || lastPosition->beatGrid != beatGrid) {
                    return;
                }
                int beatNumber = lastPosition->beatNumber;
                const int64_t beatTime = beatGrid->getTimeWithinTrack(lastPosition->beatNumber);
                const int64_t distanceIntoBeat = lastPosition->milliseconds - beatTime;
                int64_t farEnough = 0;
                if (beat.getBpm() > 0) {
                    farEnough = 6000000 / beat.getBpm() / 5;
                }
                if (distanceIntoBeat >= farEnough) {
                    beatNumber = std::min(lastPosition->beatNumber + 1, beatGrid->getBeatCount());
                }
                auto newPosition = std::make_shared<TrackPositionUpdate>(
                    beat.getTimestampNanos(),
                    timeOfBeat(*beatGrid, beatNumber, beat),
                    beatNumber,
                    true,
                    true,
                    beatlink::Util::pitchToMultiplier(beat.getPitch()),
                    false,
                    beatGrid,
                    lastPosition ? lastPosition->precise : false,
                    true);

                {
                    std::lock_guard<std::mutex> lock(positionsMutex_);
                    positions_[device] = newPosition;
                }
                updateListenersIfNeeded(device, newPosition, &beat);
            } else {
                if (lastPosition && !lastPosition->precise) {
                    {
                        std::lock_guard<std::mutex> lock(positionsMutex_);
                        positions_.erase(device);
                    }
                    updateListenersIfNeeded(device, nullptr, &beat);
                }
            }
        });

    positionListener_ = std::make_shared<beatlink::PrecisePositionCallbacks>(
        [this](const beatlink::PrecisePosition& position) {
            if (!usePrecisePositionPackets_.load()) {
                return;
            }
            const int device = position.getDeviceNumber();
            if (device >= 16) {
                return;
            }

            auto positionCopy = std::make_shared<beatlink::PrecisePosition>(position);
            {
                std::lock_guard<std::mutex> lock(updatesMutex_);
                auto it = updates_.find(device);
                if (it == updates_.end() || !it->second ||
                    it->second->getTimestampNanos() < position.getTimestampNanos()) {
                    updates_[device] = positionCopy;
                }
            }

            auto lastStatus = beatlink::VirtualCdj::getInstance().getLatestStatusFor(device);
            bool playing = false;
            bool reverse = false;
            if (lastStatus) {
                auto* cdj = dynamic_cast<beatlink::CdjStatus*>(lastStatus.get());
                if (cdj) {
                    playing = cdj->isPlaying();
                    reverse = cdj->isPlayingBackwards();
                }
            }

            auto beatGrid = BeatGridFinder::getInstance().getLatestBeatGridFor(position);
            int beatNumber = 0;
            if (beatGrid) {
                beatNumber = beatGrid->findBeatAtTime(position.getPlaybackPosition());
            }

            auto newPosition = std::make_shared<TrackPositionUpdate>(
                position.getTimestampNanos(),
                position.getPlaybackPosition(),
                beatNumber,
                true,
                playing,
                beatlink::Util::pitchToMultiplier(position.getPitch()),
                reverse,
                beatGrid,
                true,
                false);

            {
                std::lock_guard<std::mutex> lock(positionsMutex_);
                positions_[device] = newPosition;
            }
            updateListenersIfNeeded(device, newPosition, nullptr);
        });

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });
}

std::unordered_map<int, std::shared_ptr<TrackPositionUpdate>> TimeFinder::getLatestPositions() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(positionsMutex_);
    return positions_;
}

std::unordered_map<int, std::shared_ptr<beatlink::DeviceUpdate>> TimeFinder::getLatestUpdates() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(updatesMutex_);
    return updates_;
}

std::shared_ptr<TrackPositionUpdate> TimeFinder::getLatestPositionFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(positionsMutex_);
    auto it = positions_.find(player);
    return (it == positions_.end()) ? nullptr : it->second;
}

std::shared_ptr<TrackPositionUpdate> TimeFinder::getLatestPositionFor(const beatlink::DeviceUpdate& update) {
    return getLatestPositionFor(update.getDeviceNumber());
}

std::shared_ptr<beatlink::DeviceUpdate> TimeFinder::getLatestUpdateFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(updatesMutex_);
    auto it = updates_.find(player);
    return (it == updates_.end()) ? nullptr : it->second;
}

int64_t TimeFinder::getTimeFor(int player) {
    auto update = getLatestPositionFor(player);
    if (!update) {
        return -1;
    }
    return interpolateTimeSinceUpdate(*update, nowNanos());
}

int64_t TimeFinder::getTimeFor(const beatlink::DeviceUpdate& update) {
    return getTimeFor(update.getDeviceNumber());
}

void TimeFinder::addTrackPositionListener(int player, const TrackPositionListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::shared_ptr<TrackPositionUpdate> currentPosition;
    {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        auto it = positions_.find(player);
        if (it != positions_.end()) {
            currentPosition = it->second;
        }
    }

    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        auto it = std::find_if(trackPositionListeners_.begin(), trackPositionListeners_.end(),
                               [&listener](const ListenerEntry& entry) {
                                   return entry.listener == listener;
                               });
        if (it != trackPositionListeners_.end()) {
            it->player = player;
            it->lastUpdate = currentPosition;
            it->lastUpdateWasNull = !currentPosition;
        } else {
            trackPositionListeners_.push_back(
                {listener, player, currentPosition, !currentPosition});
        }
    }

    try {
        listener->movementChanged(currentPosition);
    } catch (...) {
    }
}

void TimeFinder::removeTrackPositionListener(const TrackPositionListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    trackPositionListeners_.erase(
        std::remove_if(trackPositionListeners_.begin(), trackPositionListeners_.end(),
                       [&listener](const ListenerEntry& entry) {
                           return entry.listener == listener;
                       }),
        trackPositionListeners_.end());
}

int64_t TimeFinder::getSlack() const {
    return slack_.load();
}

void TimeFinder::setSlack(int64_t slack) {
    slack_.store(slack);
}

void TimeFinder::setUsePrecisePositionPackets(bool use) {
    usePrecisePositionPackets_.store(use);
}

bool TimeFinder::isUsingPrecisePositionPackets() const {
    return usePrecisePositionPackets_.load();
}

bool TimeFinder::isOwnBeatListener(const beatlink::BeatListenerPtr& listener) const {
    return listener && beatListener_ && listener.get() == beatListener_.get();
}

void TimeFinder::start() {
    if (isRunning()) {
        return;
    }

    beatlink::DeviceFinder::getInstance().addDeviceAnnouncementListener(announcementListener_);
    BeatGridFinder::getInstance().addLifecycleListener(lifecycleListener_);
    BeatGridFinder::getInstance().start();
    beatlink::VirtualCdj::getInstance().addUpdateListener(updateListener_);
    beatlink::VirtualCdj::getInstance().addLifecycleListener(lifecycleListener_);
    beatlink::VirtualCdj::getInstance().start();
    beatlink::BeatFinder::getInstance().addLifecycleListener(lifecycleListener_);
    beatlink::BeatFinder::getInstance().addBeatListener(beatListener_);
    beatlink::BeatFinder::getInstance().addPrecisePositionListener(positionListener_);
    beatlink::BeatFinder::getInstance().start();

    running_.store(true);
    deliverLifecycleAnnouncement(true);
}

void TimeFinder::stop() {
    if (!isRunning()) {
        return;
    }

    beatlink::BeatFinder::getInstance().removePrecisePositionListener(positionListener_);
    beatlink::BeatFinder::getInstance().removeBeatListener(beatListener_);
    beatlink::VirtualCdj::getInstance().removeUpdateListener(updateListener_);

    running_.store(false);
    {
        std::lock_guard<std::mutex> lockPositions(positionsMutex_);
        positions_.clear();
    }
    {
        std::lock_guard<std::mutex> lockUpdates(updatesMutex_);
        updates_.clear();
    }

    deliverLifecycleAnnouncement(false);
}

int64_t TimeFinder::interpolateTimeSinceUpdate(const TrackPositionUpdate& update,
                                               int64_t currentTimestamp) const {
    if (!update.playing) {
        return update.milliseconds;
    }
    const int64_t elapsedMillis = (currentTimestamp - update.timestamp) / 1000000;
    const int64_t moved = static_cast<int64_t>(std::llround(update.pitch * elapsedMillis));
    if (update.reverse) {
        return update.milliseconds - moved;
    }
    return update.milliseconds + moved;
}

std::optional<CueList::Entry> TimeFinder::findAdjacentCue(
    const beatlink::CdjStatus& update,
    const std::shared_ptr<BeatGrid>& beatGrid) const {
    if (!MetadataFinder::getInstance().isRunning()) {
        return std::nullopt;
    }
    auto metadata = MetadataFinder::getInstance().getLatestMetadataFor(update);
    if (!metadata || !metadata->getCueList()) {
        return std::nullopt;
    }
    const int newBeat = update.getBeatNumber();
    for (const auto& entry : metadata->getCueList()->getEntries()) {
        const int entryBeat = beatGrid->findBeatAtTime(entry.cueTime);
        if (std::abs(newBeat - entryBeat) < 2) {
            return entry;
        }
        if (entryBeat > newBeat) {
            break;
        }
    }
    return std::nullopt;
}

int64_t TimeFinder::interpolateTimeFromUpdate(const TrackPositionUpdate& lastUpdate,
                                              const beatlink::CdjStatus& update,
                                              const std::shared_ptr<BeatGrid>& beatGrid) const {
    const int beatNumber = update.getBeatNumber();
    const bool noLongerPlaying = !update.isPlaying();

    if (lastUpdate.playing && noLongerPlaying) {
        if (auto cue = findAdjacentCue(update, beatGrid)) {
            return cue->cueTime;
        }
    }

    if (!lastUpdate.playing) {
        if (lastUpdate.beatNumber == beatNumber && noLongerPlaying) {
            return lastUpdate.milliseconds;
        }
        if (noLongerPlaying) {
            if (beatNumber < 0) {
                return -1;
            }
            return timeOfBeat(*beatGrid, beatNumber, update);
        }
    }

    const int64_t elapsedMillis = (update.getTimestampNanos() - lastUpdate.timestamp) / 1000000;
    const int64_t moved = static_cast<int64_t>(std::llround(lastUpdate.pitch * elapsedMillis));
    const int64_t interpolated = lastUpdate.reverse
        ? std::max<int64_t>(lastUpdate.milliseconds - moved, 0)
        : lastUpdate.milliseconds + moved;

    if (std::abs(beatGrid->findBeatAtTime(interpolated) - beatNumber) < 2) {
        return interpolated;
    }

    if (update.isPlayingForwards()) {
        return timeOfBeat(*beatGrid, beatNumber, update);
    }
    return beatGrid->getTimeWithinTrack(std::min(beatNumber + 1, beatGrid->getBeatCount()));
}

int64_t TimeFinder::timeOfBeat(const BeatGrid& beatGrid,
                               int beatNumber,
                               const beatlink::DeviceUpdate& update) const {
    if (beatNumber <= beatGrid.getBeatCount()) {
        return beatGrid.getTimeWithinTrack(beatNumber);
    }
    if (beatGrid.getBeatCount() < 2) {
        return beatGrid.getTimeWithinTrack(1);
    }
    const int64_t lastTime = beatGrid.getTimeWithinTrack(beatGrid.getBeatCount());
    const int64_t lastInterval = lastTime - beatGrid.getTimeWithinTrack(beatGrid.getBeatCount() - 1);
    return lastTime + (lastInterval * (beatNumber - beatGrid.getBeatCount()));
}

void TimeFinder::updateListenersIfNeeded(int player,
                                         const std::shared_ptr<TrackPositionUpdate>& update,
                                         const beatlink::Beat* beat) {
    struct PendingAction {
        TrackPositionListenerPtr listener;
        std::shared_ptr<TrackPositionUpdate> update;
        bool sendMovement{false};
        bool sendBeat{false};
        std::optional<beatlink::Beat> beatPacket;
    };

    std::vector<PendingAction> actions;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        for (auto& entry : trackPositionListeners_) {
            if (entry.player != player) {
                continue;
            }
            if (!update) {
                if (!entry.lastUpdateWasNull) {
                    entry.lastUpdate = nullptr;
                    entry.lastUpdateWasNull = true;
                    actions.push_back({entry.listener, nullptr, true, false, std::nullopt});
                }
                continue;
            }

            const bool needsUpdate = entry.lastUpdateWasNull ||
                                     !entry.lastUpdate ||
                                     entry.lastUpdate->playing != update->playing ||
                                     pitchesDiffer(*entry.lastUpdate, *update) ||
                                     interpolationsDisagree(*entry.lastUpdate, *update);

            if (needsUpdate) {
                entry.lastUpdate = update;
                entry.lastUpdateWasNull = false;
                actions.push_back({entry.listener, update, true, false, std::nullopt});
            }

            if (update->fromBeat && beat) {
                auto* beatListener = dynamic_cast<TrackPositionBeatListener*>(entry.listener.get());
                if (beatListener) {
                    PendingAction action;
                    action.listener = entry.listener;
                    action.update = update;
                    action.sendBeat = true;
                    action.beatPacket = *beat;
                    actions.push_back(std::move(action));
                }
            }
        }
    }

    for (const auto& action : actions) {
        if (!action.listener) {
            continue;
        }
        if (action.sendMovement) {
            try {
                action.listener->movementChanged(action.update);
            } catch (...) {
            }
        }
        if (action.sendBeat && action.beatPacket) {
            auto* beatListener = dynamic_cast<TrackPositionBeatListener*>(action.listener.get());
            if (beatListener) {
                try {
                    beatListener->newBeat(*action.beatPacket, action.update);
                } catch (...) {
                }
            }
        }
    }
}

bool TimeFinder::interpolationsDisagree(const TrackPositionUpdate& lastUpdate,
                                        const TrackPositionUpdate& currentUpdate) const {
    const int64_t now = nowNanos();
    const int64_t skew = std::llabs(
        interpolateTimeSinceUpdate(lastUpdate, now) -
        interpolateTimeSinceUpdate(currentUpdate, now));
    const int64_t tolerance = lastUpdate.playing ? slack_.load() : 0;
    return skew > tolerance;
}

bool TimeFinder::pitchesDiffer(const TrackPositionUpdate& lastUpdate,
                               const TrackPositionUpdate& currentUpdate) const {
    const double delta = std::abs(lastUpdate.pitch - currentUpdate.pitch);
    if (lastUpdate.precise && (lastUpdate.fromBeat != currentUpdate.fromBeat)) {
        return delta > 0.001;
    }
    return delta > 0.000001;
}

} // namespace beatlink::data
