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
#include "PacketTypes.hpp"
#include "Util.hpp"

namespace beatlink {

/**
 * Callback type for beat events.
 */
using BeatCallback = std::function<void(const Beat&)>;

/**
 * Watches for beat packets on port 50001.
 * Ported from org.deepsymmetry.beatlink.BeatFinder
 */
class BeatFinder {
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
    bool isRunning() const { return running_.load(); }

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

    std::atomic<bool> running_{false};

    asio::io_context ioContext_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    std::thread receiverThread_;

    mutable std::mutex listenersMutex_;
    std::vector<BeatCallback> beatListeners_;

    std::array<uint8_t, 512> receiveBuffer_;
    asio::ip::udp::endpoint senderEndpoint_;
};

} // namespace beatlink
