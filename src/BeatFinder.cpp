#include "beatlink/BeatFinder.hpp"
#include "beatlink/DeviceFinder.hpp"
#include "beatlink/VirtualCdj.hpp"
#include "beatlink/data/TimeFinder.hpp"
#include <algorithm>
#include <iostream>
#include <set>

namespace beatlink {

BeatFinder::BeatFinder() {
}

BeatFinder::~BeatFinder() {
    stop();
}

bool BeatFinder::start() {
    if (running_.load()) {
        return true;
    }

    try {
        socket_ = std::make_unique<asio::ip::udp::socket>(ioContext_);
        socket_->open(asio::ip::udp::v4());
        socket_->set_option(asio::socket_base::reuse_address(true));
        // Enable SO_REUSEPORT to allow sharing port with Rekordbox on same machine
        #ifdef SO_REUSEPORT
        int reuse = 1;
        setsockopt(socket_->native_handle(), SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
        #endif
        socket_->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), Ports::BEAT));

        running_.store(true);
        receiverThread_ = std::thread(&BeatFinder::receiverLoop, this);
        deliverLifecycleAnnouncement(true);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "BeatFinder: Failed to start: " << e.what() << std::endl;
        socket_.reset();
        return false;
    }
}

void BeatFinder::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (socket_) {
        asio::error_code ec;
        socket_->close(ec);
    }

    if (receiverThread_.joinable()) {
        receiverThread_.join();
    }

    socket_.reset();
    deliverLifecycleAnnouncement(false);
}

void BeatFinder::receiverLoop() {
    while (running_.load()) {
        try {
            socket_->non_blocking(true);

            asio::error_code ec;
            size_t length = socket_->receive_from(
                asio::buffer(receiveBuffer_), senderEndpoint_, 0, ec);

            if (ec == asio::error::would_block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (ec) {
                if (running_.load()) {
                    std::cerr << "BeatFinder: Receive error: " << ec.message() << std::endl;
                }
                continue;
            }

            // Check if address is ignored
            auto senderAddr = senderEndpoint_.address().to_v4();
            if (DeviceFinder::getInstance().isAddressIgnored(senderAddr)) {
                continue;
            }

            // Process the packet
            processPacket(receiveBuffer_.data(), length, senderAddr);

        } catch (const std::exception& e) {
            if (running_.load()) {
                std::cerr << "BeatFinder: Exception in receiver: " << e.what() << std::endl;
            }
        }
    }
}

void BeatFinder::processPacket(const uint8_t* data, size_t length, const asio::ip::address_v4& sender) {
    auto type = Util::validateHeader(std::span<const uint8_t>(data, length), Ports::BEAT);
    if (!type) {
        return;
    }

    switch (*type) {
        case PacketType::BEAT: {
            if (length < Beat::PACKET_SIZE) {
                std::cerr << "BeatFinder: Ignoring too-short beat packet" << std::endl;
                return;
            }
            try {
                Beat beat(data, Beat::PACKET_SIZE, sender);
                deliverBeat(beat);
            } catch (const std::exception& e) {
                std::cerr << "BeatFinder: Error processing beat: " << e.what() << std::endl;
            }
            break;
        }
        case PacketType::PRECISE_POSITION: {
            if (length < PrecisePosition::PACKET_SIZE) {
                std::cerr << "BeatFinder: Ignoring too-short precise position packet" << std::endl;
                return;
            }
            try {
                PrecisePosition position(data, PrecisePosition::PACKET_SIZE, sender);
                deliverPrecisePosition(position);
            } catch (const std::exception& e) {
                std::cerr << "BeatFinder: Error processing precise position: " << e.what() << std::endl;
            }
            break;
        }
        case PacketType::CHANNELS_ON_AIR: {
            if (length < 0x2d) {
                std::cerr << "BeatFinder: Ignoring too-short channels-on-air packet" << std::endl;
                return;
            }
            std::set<int> audibleChannels;
            for (int channel = 1; channel <= 4; ++channel) {
                if (data[0x23 + channel] != 0) {
                    audibleChannels.insert(channel);
                }
            }
            if (length >= 0x35) {
                for (int channel = 5; channel <= 6; ++channel) {
                    if (data[0x28 + channel] != 0) {
                        audibleChannels.insert(channel);
                    }
                }
            }
            deliverOnAirUpdate(audibleChannels);
            break;
        }
        case PacketType::SYNC_CONTROL: {
            if (length < 0x2c) {
                std::cerr << "BeatFinder: Ignoring too-short sync control packet" << std::endl;
                return;
            }
            deliverSyncCommand(data[0x2b]);
            break;
        }
        case PacketType::MASTER_HANDOFF_REQUEST: {
            if (length < 0x28) {
                std::cerr << "BeatFinder: Ignoring too-short master handoff request" << std::endl;
                return;
            }
            deliverMasterYieldCommand(data[0x21]);
            break;
        }
        case PacketType::MASTER_HANDOFF_RESPONSE: {
            if (length < 0x2c) {
                std::cerr << "BeatFinder: Ignoring too-short master handoff response" << std::endl;
                return;
            }
            deliverMasterYieldResponse(data[0x21], data[0x2b] == 1);
            break;
        }
        case PacketType::FADER_START_COMMAND: {
            if (length < 0x28) {
                std::cerr << "BeatFinder: Ignoring too-short fader start command" << std::endl;
                return;
            }
            std::set<int> playersToStart;
            std::set<int> playersToStop;
            for (int channel = 1; channel <= 4; ++channel) {
                switch (data[0x23 + channel]) {
                    case 0:
                        playersToStart.insert(channel);
                        break;
                    case 1:
                        playersToStop.insert(channel);
                        break;
                    case 2:
                        break;
                    default:
                        std::cerr << "BeatFinder: Unrecognized fader start command value" << std::endl;
                        break;
                }
            }
            deliverFaderStartCommand(playersToStart, playersToStop);
            break;
        }
        default:
            break;
    }
}

void BeatFinder::addBeatListener(BeatCallback callback) {
    addBeatListener(std::make_shared<BeatListenerCallbacks>(std::move(callback)));
}

void BeatFinder::addBeatListener(const BeatListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    if (beatlink::data::TimeFinder::getInstance().isOwnBeatListener(listener)) {
        timeFinderBeatListener_ = listener;
    } else {
        beatListeners_.push_back(listener);
    }
}

void BeatFinder::removeBeatListener(const BeatListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    if (timeFinderBeatListener_ == listener) {
        timeFinderBeatListener_.reset();
        return;
    }
    beatListeners_.erase(
        std::remove(beatListeners_.begin(), beatListeners_.end(), listener),
        beatListeners_.end());
}

void BeatFinder::addPrecisePositionListener(const std::shared_ptr<PrecisePositionListener>& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    precisePositionListeners_.push_back(listener);
}

void BeatFinder::removePrecisePositionListener(const std::shared_ptr<PrecisePositionListener>& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    precisePositionListeners_.erase(
        std::remove(precisePositionListeners_.begin(), precisePositionListeners_.end(), listener),
        precisePositionListeners_.end());
}

void BeatFinder::addSyncListener(const std::shared_ptr<SyncListener>& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    syncListeners_.push_back(listener);
}

void BeatFinder::addOnAirListener(const std::shared_ptr<OnAirListener>& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    onAirListeners_.push_back(listener);
}

void BeatFinder::addFaderStartListener(const std::shared_ptr<FaderStartListener>& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    faderStartListeners_.push_back(listener);
}

void BeatFinder::addMasterHandoffListener(const std::shared_ptr<MasterHandoffListener>& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    masterHandoffListeners_.push_back(listener);
}

void BeatFinder::clearListeners() {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    beatListeners_.clear();
    timeFinderBeatListener_.reset();
    precisePositionListeners_.clear();
    syncListeners_.clear();
    onAirListeners_.clear();
    faderStartListeners_.clear();
    masterHandoffListeners_.clear();
}

void BeatFinder::deliverBeat(const Beat& beat) {
    VirtualCdj::getInstance().processBeat(beat);

    std::vector<BeatListenerPtr> listeners;
    BeatListenerPtr timeFinderListener;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = beatListeners_;
        timeFinderListener = timeFinderBeatListener_;
    }

    if (timeFinderListener) {
        try {
            timeFinderListener->newBeat(beat);
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in TimeFinder beat listener: " << e.what() << std::endl;
        }
    }

    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->newBeat(beat);
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in beat listener: " << e.what() << std::endl;
        }
    }
}

void BeatFinder::deliverPrecisePosition(const PrecisePosition& position) {
    std::vector<std::shared_ptr<PrecisePositionListener>> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = precisePositionListeners_;
    }

    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->positionReported(position);
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in precise position listener: " << e.what() << std::endl;
        }
    }
}

void BeatFinder::deliverOnAirUpdate(const std::set<int>& audibleChannels) {
    std::vector<std::shared_ptr<OnAirListener>> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = onAirListeners_;
    }

    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->channelsOnAir(audibleChannels);
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in on-air listener: " << e.what() << std::endl;
        }
    }
}

void BeatFinder::deliverSyncCommand(uint8_t payload) {
    std::vector<std::shared_ptr<SyncListener>> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = syncListeners_;
    }

    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            switch (payload) {
                case 0x01:
                    listener->becomeMaster();
                    break;
                case 0x10:
                    listener->setSyncMode(true);
                    break;
                case 0x20:
                    listener->setSyncMode(false);
                    break;
                default:
                    break;
            }
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in sync listener: " << e.what() << std::endl;
        }
    }
}

void BeatFinder::deliverMasterYieldCommand(uint8_t deviceNumber) {
    std::vector<std::shared_ptr<MasterHandoffListener>> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = masterHandoffListeners_;
    }

    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->yieldMasterTo(static_cast<int>(deviceNumber));
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in master handoff listener: " << e.what() << std::endl;
        }
    }
}

void BeatFinder::deliverMasterYieldResponse(uint8_t deviceNumber, bool yielded) {
    std::vector<std::shared_ptr<MasterHandoffListener>> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = masterHandoffListeners_;
    }

    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->yieldResponse(static_cast<int>(deviceNumber), yielded);
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in master handoff listener: " << e.what() << std::endl;
        }
    }
}

void BeatFinder::deliverFaderStartCommand(const std::set<int>& playersToStart, const std::set<int>& playersToStop) {
    std::vector<std::shared_ptr<FaderStartListener>> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = faderStartListeners_;
    }

    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->fadersChanged(playersToStart, playersToStop);
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in fader start listener: " << e.what() << std::endl;
        }
    }
}

} // namespace beatlink
