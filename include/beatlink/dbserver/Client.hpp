#pragma once

#include "DataReader.hpp"
#include "Field.hpp"
#include "Message.hpp"
#include "NumberField.hpp"

#include "beatlink/CdjStatus.hpp"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace beatlink::dbserver {

class Client {
public:
    static constexpr int DEFAULT_MENU_BATCH_SIZE = 64;
    static const NumberField GREETING_FIELD;

    Client(const asio::ip::address_v4& address, int port, int targetPlayer, int posingAsPlayer,
           std::chrono::milliseconds timeout);
    ~Client();

    int targetPlayer() const { return targetPlayer_; }
    int posingAsPlayer() const { return posingAsPlayer_; }

    bool isConnected() const;
    void close();

    static NumberField buildRMST(int requestingPlayer, Message::MenuIdentifier targetMenu,
                                 TrackSourceSlot slot);
    static NumberField buildRMST(int requestingPlayer, Message::MenuIdentifier targetMenu,
                                 TrackSourceSlot slot, TrackType trackType);

    NumberField buildRMST(Message::MenuIdentifier targetMenu, TrackSourceSlot slot);
    NumberField buildRMST(Message::MenuIdentifier targetMenu, TrackSourceSlot slot, TrackType trackType);

    Message simpleRequest(Message::KnownType requestType, std::optional<Message::KnownType> responseType,
                          const std::vector<FieldPtr>& arguments);

    Message menuRequest(Message::KnownType requestType, Message::MenuIdentifier targetMenu,
                        TrackSourceSlot slot, const std::vector<FieldPtr>& arguments);

    Message menuRequestTyped(Message::KnownType requestType, Message::MenuIdentifier targetMenu,
                             TrackSourceSlot slot, TrackType trackType, const std::vector<FieldPtr>& arguments);

    static int getMenuBatchSize();
    static void setMenuBatchSize(int batchSize);

    bool tryLockingForMenuOperations(std::chrono::milliseconds timeout);
    void unlockForMenuOperations();

    std::vector<Message> renderMenuItems(Message::MenuIdentifier targetMenu, TrackSourceSlot slot,
                                         TrackType trackType, const Message& availableResponse);

    std::vector<Message> renderMenuItems(Message::MenuIdentifier targetMenu, TrackSourceSlot slot,
                                         TrackType trackType, int offset, int count);

    std::string toString() const;

private:
    void performSetupExchange();
    void performTeardownExchange();

    void sendField(const Field& field);
    void sendMessage(const Message& message);

    NumberField assignTransactionNumber();

    bool isMenuLockedByCurrentThread() const;

    asio::io_context ioContext_;
    asio::ip::tcp::socket socket_;
    DataReader reader_;
    std::chrono::milliseconds timeout_;

    int targetPlayer_;
    int posingAsPlayer_;
    int64_t transactionCounter_{0};

    mutable std::recursive_mutex requestMutex_;
    std::timed_mutex menuLock_;
    mutable std::mutex menuOwnerMutex_;
    std::thread::id menuOwner_{};

    static std::atomic<int> menuBatchSize_;
};

} // namespace beatlink::dbserver
