#include "beatlink/BeatFinder.hpp"
#include "beatlink/DeviceFinder.hpp"
#include <iostream>

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
    // Validate header
    if (!Util::validateHeader(data, length)) {
        return;
    }

    // Get packet type
    uint8_t packetType = Util::getPacketType(data, length);
    auto type = PacketTypes::lookup(Ports::BEAT, packetType);

    if (!type) {
        return;
    }

    if (*type == PacketType::BEAT) {
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
    }
    // Other packet types (SYNC_CONTROL, CHANNELS_ON_AIR, etc.) can be added here
}

void BeatFinder::addBeatListener(BeatCallback callback) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    beatListeners_.push_back(std::move(callback));
}

void BeatFinder::clearListeners() {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    beatListeners_.clear();
}

void BeatFinder::deliverBeat(const Beat& beat) {
    std::vector<BeatCallback> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = beatListeners_;
    }

    for (const auto& listener : listeners) {
        try {
            listener(beat);
        } catch (const std::exception& e) {
            std::cerr << "BeatFinder: Exception in beat listener: " << e.what() << std::endl;
        }
    }
}

} // namespace beatlink
