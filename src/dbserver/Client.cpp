#include "beatlink/dbserver/Client.hpp"

#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/StringField.hpp"

#include <algorithm>
#include <stdexcept>

namespace beatlink::dbserver {

const NumberField Client::GREETING_FIELD = NumberField(1, 4);
std::atomic<int> Client::menuBatchSize_{Client::DEFAULT_MENU_BATCH_SIZE};

Client::Client(const asio::ip::address_v4& address, int port, int targetPlayer, int posingAsPlayer,
               std::chrono::milliseconds timeout)
    : socket_(ioContext_)
    , reader_(socket_, timeout)
    , timeout_(timeout)
    , targetPlayer_(targetPlayer)
    , posingAsPlayer_(posingAsPlayer)
{
    asio::ip::tcp::endpoint endpoint(address, port);
    socket_.connect(endpoint);

    try {
        sendField(GREETING_FIELD);
        auto response = Field::read(reader_);
        auto responseNumber = std::dynamic_pointer_cast<NumberField>(response);
        if (responseNumber && responseNumber->getSize() == 4 && responseNumber->getValue() == 1) {
            performSetupExchange();
        } else {
            throw std::runtime_error("Did not receive expected greeting response from dbserver");
        }
    } catch (...) {
        close();
        throw;
    }
}

Client::~Client() {
    close();
}

bool Client::isConnected() const {
    return socket_.is_open();
}

void Client::close() {
    if (!socket_.is_open()) {
        return;
    }
    try {
        performTeardownExchange();
    } catch (...) {
    }
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
}

void Client::sendField(const Field& field) {
    if (!isConnected()) {
        throw std::runtime_error("sendField() called after dbserver connection was closed");
    }
    field.write(socket_);
}

void Client::sendMessage(const Message& message) {
    size_t totalSize = 0;
    for (const auto& field : message.fields) {
        totalSize += field->getBytes().size();
    }
    std::vector<uint8_t> combined;
    combined.reserve(totalSize);
    for (const auto& field : message.fields) {
        auto bytes = field->getBytes();
        combined.insert(combined.end(), bytes.begin(), bytes.end());
    }
    if (!combined.empty()) {
        asio::write(socket_, asio::buffer(combined.data(), combined.size()));
    }
}

NumberField Client::assignTransactionNumber() {
    return NumberField(++transactionCounter_, 4);
}

void Client::performSetupExchange() {
    Message setupRequest(0xfffffffeLL, Message::KnownType::SETUP_REQ,
                         {std::make_shared<NumberField>(posingAsPlayer_, 4)});
    sendMessage(setupRequest);
    Message response = Message::read(reader_);
    if (!response.knownType || *response.knownType != Message::KnownType::MENU_AVAILABLE) {
        throw std::runtime_error("Did not receive message type 0x4000 in response to setup message");
    }
    if (response.arguments.size() != 2) {
        throw std::runtime_error("Did not receive two arguments in response to setup message");
    }
    auto playerField = std::dynamic_pointer_cast<NumberField>(response.arguments.at(1));
    if (!playerField) {
        throw std::runtime_error("Second argument in response to setup message was not a number");
    }
    if (playerField->getValue() != targetPlayer_) {
        throw std::runtime_error("Welcome response identified wrong player");
    }
}

void Client::performTeardownExchange() {
    Message teardownRequest(0xfffffffeLL, Message::KnownType::TEARDOWN_REQ, {});
    sendMessage(teardownRequest);
}

NumberField Client::buildRMST(int requestingPlayer, Message::MenuIdentifier targetMenu,
                              TrackSourceSlot slot) {
    return buildRMST(requestingPlayer, targetMenu, slot, TrackType::REKORDBOX);
}

NumberField Client::buildRMST(int requestingPlayer, Message::MenuIdentifier targetMenu,
                              TrackSourceSlot slot, TrackType trackType) {
    const auto value =
        (static_cast<int64_t>((requestingPlayer & 0xff) << 24)) |
        (static_cast<int64_t>((static_cast<uint8_t>(targetMenu) & 0xff) << 16)) |
        (static_cast<int64_t>((static_cast<uint8_t>(slot) & 0xff) << 8)) |
        (static_cast<int64_t>(static_cast<uint8_t>(trackType) & 0xff));
    return NumberField(value, 4);
}

NumberField Client::buildRMST(Message::MenuIdentifier targetMenu, TrackSourceSlot slot) {
    return buildRMST(posingAsPlayer_, targetMenu, slot, TrackType::REKORDBOX);
}

NumberField Client::buildRMST(Message::MenuIdentifier targetMenu, TrackSourceSlot slot, TrackType trackType) {
    return buildRMST(posingAsPlayer_, targetMenu, slot, trackType);
}

Message Client::simpleRequest(Message::KnownType requestType, std::optional<Message::KnownType> responseType,
                              const std::vector<FieldPtr>& arguments) {
    std::lock_guard<std::recursive_mutex> lock(requestMutex_);
    const NumberField transaction = assignTransactionNumber();
    Message request(transaction.getValue(), requestType, arguments);
    sendMessage(request);
    Message response = Message::read(reader_);
    if (response.transaction.getValue() != transaction.getValue()) {
        throw std::runtime_error("Received response with wrong transaction ID");
    }
    if (responseType && (!response.knownType || *response.knownType != *responseType)) {
        throw std::runtime_error("Received response with wrong type");
    }
    return response;
}

Message Client::menuRequest(Message::KnownType requestType, Message::MenuIdentifier targetMenu,
                            TrackSourceSlot slot, const std::vector<FieldPtr>& arguments) {
    return menuRequestTyped(requestType, targetMenu, slot, TrackType::REKORDBOX, arguments);
}

Message Client::menuRequestTyped(Message::KnownType requestType, Message::MenuIdentifier targetMenu,
                                 TrackSourceSlot slot, TrackType trackType, const std::vector<FieldPtr>& arguments) {
    if (!isMenuLockedByCurrentThread()) {
        throw std::runtime_error("menuRequestTyped() cannot be called without holding menu lock");
    }

    std::vector<FieldPtr> combined;
    combined.reserve(arguments.size() + 1);
    combined.push_back(std::make_shared<NumberField>(buildRMST(targetMenu, slot, trackType)));
    combined.insert(combined.end(), arguments.begin(), arguments.end());

    Message response = simpleRequest(requestType, Message::KnownType::MENU_AVAILABLE, combined);
    auto reportedRequestType = std::dynamic_pointer_cast<NumberField>(response.arguments.at(0));
    if (!reportedRequestType || reportedRequestType->getValue() != static_cast<int64_t>(static_cast<uint16_t>(requestType))) {
        throw std::runtime_error("Menu request did not return result for same type as request");
    }
    return response;
}

int Client::getMenuBatchSize() {
    return menuBatchSize_.load();
}

void Client::setMenuBatchSize(int batchSize) {
    menuBatchSize_.store(batchSize);
}

bool Client::tryLockingForMenuOperations(std::chrono::milliseconds timeout) {
    if (menuLock_.try_lock_for(timeout)) {
        std::lock_guard<std::mutex> lock(menuOwnerMutex_);
        menuOwner_ = std::this_thread::get_id();
        return true;
    }
    return false;
}

void Client::unlockForMenuOperations() {
    {
        std::lock_guard<std::mutex> lock(menuOwnerMutex_);
        menuOwner_ = std::thread::id();
    }
    menuLock_.unlock();
}

bool Client::isMenuLockedByCurrentThread() const {
    std::lock_guard<std::mutex> lock(menuOwnerMutex_);
    return menuOwner_ == std::this_thread::get_id();
}

std::vector<Message> Client::renderMenuItems(Message::MenuIdentifier targetMenu, TrackSourceSlot slot,
                                             TrackType trackType, const Message& availableResponse) {
    const auto count = availableResponse.getMenuResultsCount();
    if (count == Message::NO_MENU_RESULTS_AVAILABLE || count == 0) {
        return {};
    }
    return renderMenuItems(targetMenu, slot, trackType, 0, static_cast<int>(count));
}

std::vector<Message> Client::renderMenuItems(Message::MenuIdentifier targetMenu, TrackSourceSlot slot,
                                             TrackType trackType, int offset, int count) {
    std::lock_guard<std::recursive_mutex> lock(requestMutex_);
    if (!isMenuLockedByCurrentThread()) {
        throw std::runtime_error("renderMenuItems() cannot be called without holding menu lock");
    }
    if (offset < 0) {
        throw std::invalid_argument("offset must be nonnegative");
    }
    if (count < 1) {
        throw std::invalid_argument("count must be positive");
    }

    std::vector<Message> results;
    results.reserve(static_cast<size_t>(count));

    int gathered = 0;
    int currentOffset = offset;
    while (gathered < count) {
        const int batchSize = std::min(count - gathered, menuBatchSize_.load());
        const NumberField transaction = assignTransactionNumber();
        const auto limit = std::make_shared<NumberField>(batchSize);
        const auto total = std::make_shared<NumberField>(count);

        Message request(transaction.getValue(), Message::KnownType::RENDER_MENU_REQ,
                        {std::make_shared<NumberField>(buildRMST(targetMenu, slot, trackType)),
                         std::make_shared<NumberField>(currentOffset),
                         limit,
                         std::make_shared<NumberField>(NumberField::WORD_0),
                         total,
                         std::make_shared<NumberField>(NumberField::WORD_0)});

        sendMessage(request);

        Message response = Message::read(reader_);
        if (response.transaction.getValue() != transaction.getValue()) {
            throw std::runtime_error("Received response with wrong transaction ID");
        }
        if (!response.knownType || *response.knownType != Message::KnownType::MENU_HEADER) {
            throw std::runtime_error("Expecting MENU_HEADER in response to render menu request");
        }

        response = Message::read(reader_);
        while (response.knownType && *response.knownType == Message::KnownType::MENU_ITEM) {
            results.push_back(response);
            response = Message::read(reader_);
        }

        if (!response.knownType || *response.knownType != Message::KnownType::MENU_FOOTER) {
            throw std::runtime_error("Expecting MENU_FOOTER after MENU_ITEM responses");
        }

        currentOffset += batchSize;
        gathered += batchSize;
    }
    return results;
}

std::string Client::toString() const {
    return "DBServer Client[targetPlayer: " + std::to_string(targetPlayer_) +
           ", posingAsPlayer: " + std::to_string(posingAsPlayer_) +
           ", transactionCounter: " + std::to_string(transactionCounter_) +
           ", menuBatchSize: " + std::to_string(menuBatchSize_.load()) + "]";
}

} // namespace beatlink::dbserver
