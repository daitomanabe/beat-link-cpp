#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <asio.hpp>

#include "Beat.hpp"
#include "BeatListener.hpp"
#include "FaderStartListener.hpp"
#include "LifecycleParticipant.hpp"
#include "MasterHandoffListener.hpp"
#include "OnAirListener.hpp"
#include "PrecisePosition.hpp"
#include "PrecisePositionListener.hpp"
#include "SyncListener.hpp"
#include "PacketTypes.hpp"
#include "Util.hpp"

namespace beatlink {

/**
 * Watches for beat packets on port 50001.
 * Ported from org.deepsymmetry.beatlink.BeatFinder
 */
class BeatFinder : public LifecycleParticipant {
public:
    /**
     * Get the singleton instance.
     */
    static BeatFinder& getInstance() {
        static BeatFinder instance;
        return instance;
    }

    /**
     * Check if the finder is running.
     */
    bool isRunning() const override { return running_.load(); }

    /**
     * Start listening for beat packets.
     *
     * @return true if started successfully
     */
    bool start();

    /**
     * Stop listening for beat packets.
     */
    void stop();

    /**
     * Add a listener for beat events.
     */
    void addBeatListener(BeatCallback callback);
    void addBeatListener(const BeatListenerPtr& listener);
    void removeBeatListener(const BeatListenerPtr& listener);

    void addPrecisePositionListener(const std::shared_ptr<PrecisePositionListener>& listener);
    void removePrecisePositionListener(const std::shared_ptr<PrecisePositionListener>& listener);
    void addSyncListener(const std::shared_ptr<SyncListener>& listener);
    void addOnAirListener(const std::shared_ptr<OnAirListener>& listener);
    void addFaderStartListener(const std::shared_ptr<FaderStartListener>& listener);
    void addMasterHandoffListener(const std::shared_ptr<MasterHandoffListener>& listener);

    /**
     * Clear all listeners.
     */
    void clearListeners();

    // Delete copy/move
    BeatFinder(const BeatFinder&) = delete;
    BeatFinder& operator=(const BeatFinder&) = delete;

private:
    BeatFinder();
    ~BeatFinder();

    void receiverLoop();
    void processPacket(const uint8_t* data, size_t length, const asio::ip::address_v4& sender);
    void deliverBeat(const Beat& beat);
    void deliverPrecisePosition(const PrecisePosition& position);
    void deliverOnAirUpdate(const std::set<int>& audibleChannels);
    void deliverSyncCommand(uint8_t payload);
    void deliverMasterYieldCommand(uint8_t deviceNumber);
    void deliverMasterYieldResponse(uint8_t deviceNumber, bool yielded);
    void deliverFaderStartCommand(const std::set<int>& playersToStart, const std::set<int>& playersToStop);

    std::atomic<bool> running_{false};

    asio::io_context ioContext_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    std::thread receiverThread_;

    mutable std::mutex listenersMutex_;
    std::vector<BeatListenerPtr> beatListeners_;
    BeatListenerPtr timeFinderBeatListener_;
    std::vector<std::shared_ptr<PrecisePositionListener>> precisePositionListeners_;
    std::vector<std::shared_ptr<SyncListener>> syncListeners_;
    std::vector<std::shared_ptr<OnAirListener>> onAirListeners_;
    std::vector<std::shared_ptr<FaderStartListener>> faderStartListeners_;
    std::vector<std::shared_ptr<MasterHandoffListener>> masterHandoffListeners_;

    std::array<uint8_t, 512> receiveBuffer_;
    asio::ip::udp::endpoint senderEndpoint_;
};

} // namespace beatlink
