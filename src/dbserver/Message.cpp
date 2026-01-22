#include "beatlink/dbserver/Message.hpp"
#include "beatlink/dbserver/DataReader.hpp"

#include <fmt/format.h>
#include <sstream>
#include <stdexcept>

namespace beatlink::dbserver {

const NumberField Message::MESSAGE_START = NumberField(0x872349ae, 4);

static const std::vector<Message::KnownTypeInfo>& knownTypeInfoList() {
    static const std::vector<Message::KnownTypeInfo> info = {
        {Message::KnownType::SETUP_REQ, 0x0000, "setup request", {"requesting player"}},
        {Message::KnownType::INVALID_DATA, 0x0001, "invalid data", {}},
        {Message::KnownType::TEARDOWN_REQ, 0x0100, "teardown request", {}},
        {Message::KnownType::ROOT_MENU_REQ, 0x1000, "root menu request", {"r:m:s:t", "sort order", "magic constant?"}},
        {Message::KnownType::GENRE_MENU_REQ, 0x1001, "genre menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::ARTIST_MENU_REQ, 0x1002, "artist menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::ALBUM_MENU_REQ, 0x1003, "album menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::TRACK_MENU_REQ, 0x1004, "track menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::BPM_MENU_REQ, 0x1006, "bpm menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::RATING_MENU_REQ, 0x1007, "rating menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::YEAR_MENU_REQ, 0x1008, "year menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::LABEL_MENU_REQ, 0x100a, "label menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::COLOR_MENU_REQ, 0x100d, "color menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::TIME_MENU_REQ, 0x1010, "time menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::BIT_RATE_MENU_REQ, 0x1011, "bit rate menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::HISTORY_MENU_REQ, 0x1012, "history menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::FILENAME_MENU_REQ, 0x1013, "filename menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::KEY_MENU_REQ, 0x1014, "key menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::ARTIST_MENU_FOR_GENRE_REQ, 0x1101, "artist menu for genre request", {"r:m:s:t", "sort", "genre ID"}},
        {Message::KnownType::ALBUM_MENU_FOR_ARTIST_REQ, 0x1102, "album menu for artist request", {"r:m:s:t", "sort", "artist ID"}},
        {Message::KnownType::TRACK_MENU_FOR_ALBUM_REQ, 0x1103, "track menu for album request", {"r:m:s:t", "sort", "album ID"}},
        {Message::KnownType::PLAYLIST_REQ, 0x1105, "playlist/folder request", {"r:m:s:t", "sort order", "playlist/folder ID", "0=playlist, 1=folder"}},
        {Message::KnownType::BPM_RANGE_REQ, 0x1106, "bpm range request", {"r:m:s:t", "sort order", "tempo"}},
        {Message::KnownType::TRACK_MENU_FOR_RATING_REQ, 0x1107, "track menu for rating request", {"r:m:s:t", "sort", "rating ID"}},
        {Message::KnownType::YEAR_MENU_FOR_DECADE_REQ, 0x1108, "year menu for decade request", {"r:m:s:t", "sort", "decade"}},
        {Message::KnownType::ARTIST_MENU_FOR_LABEL_REQ, 0x110a, "artist menu for genre request", {"r:m:s:t", "sort", "label ID, or -1 for ALL"}},
        {Message::KnownType::TRACK_MENU_FOR_COLOR_REQ, 0x110d, "track menu for color request", {"r:m:s:t", "sort", "color ID"}},
        {Message::KnownType::TRACK_MENU_FOR_TIME_REQ, 0x1110, "track menu for time request", {"r:m:s:t", "sort", "minutes"}},
        {Message::KnownType::TRACK_MENU_FOR_BIT_RATE_REQ, 0x1111, "track menu for bit rate request", {"r:m:s:t", "sort", "bit rate"}},
        {Message::KnownType::TRACK_MENU_FOR_HISTORY_REQ, 0x1112, "track menu for history entry request", {"r:m:s:t", "sort", "history ID"}},
        {Message::KnownType::NEIGHBOR_MENU_FOR_KEY, 0x1114, "neighbor menu for key request", {"r:m:s:t", "sort", "key ID"}},
        {Message::KnownType::ALBUM_MENU_FOR_GENRE_AND_ARTIST, 0x1201, "album menu for genre and artist request", {"r:m:s:t:", "sort", "genre ID", "artist ID, or -1 for ALL"}},
        {Message::KnownType::TRACK_MENU_FOR_ARTIST_AND_ALBUM, 0x1202, "track menu for artist and album request", {"r:m:s:t:", "sort", "artist ID", "album ID, or -1 for ALL"}},
        {Message::KnownType::TRACK_MENU_FOR_BPM_AND_DISTANCE, 0x1206, "track menu for BPM and distance request", {"r:m:s:t:", "sort", "bpm ID", "distance (+/- %, can range from 0-6)"}},
        {Message::KnownType::TRACK_MENU_FOR_DECADE_YEAR_REQ, 0x1208, "track menu for decade and year request", {"r:m:s:t", "sort", "decade", "year, or -1 for ALL"}},
        {Message::KnownType::ALBUM_MENU_FOR_LABEL_AND_ARTIST, 0x120a, "album menu for label and artist request", {"r:m:s:t:", "sort", "label ID", "artist ID, or -1 for ALL"}},
        {Message::KnownType::TRACK_MENU_FOR_KEY_AND_DISTANCE, 0x1214, "track menu for key and distance request", {"r:m:s:t:", "sort", "key ID", "distance (around circle of fifths)"}},
        {Message::KnownType::SEARCH_MENU, 0x1300, "search by substring request", {"r:m:s:t", "sort", "search string byte size", "search string (must be uppercase", "unknown (0)"}},
        {Message::KnownType::TRACK_MENU_FOR_GENRE_ARTIST_AND_ALBUM, 0x1301, "track menu for genre, artist and album request", {"r:m:s:t:", "sort", "genre ID", "artist ID, or -1 for ALL", "album ID, or -1 for ALL"}},
        {Message::KnownType::ORIGINAL_ARTIST_MENU_REQ, 0x1302, "original artist menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::TRACK_MENU_FOR_LABEL_ARTIST_AND_ALBUM, 0x130a, "track menu for label, artist and album request", {"r:m:s:t:", "sort", "label ID", "artist ID, or -1 for ALL", "album ID, or -1 for ALL"}},
        {Message::KnownType::ALBUM_MENU_FOR_ORIGINAL_ARTIST_REQ, 0x1402, "album menu for original artist request", {"r:m:s:t", "sort", "artist ID"}},
        {Message::KnownType::TRACK_MENU_FOR_ORIGINAL_ARTIST_AND_ALBUM, 0x1502, "track menu for original artist and album request", {"r:m:s:t:", "sort", "artist ID", "album ID, or -1 for ALL"}},
        {Message::KnownType::REMIXER_MENU_REQ, 0x1602, "remixer menu request", {"r:m:s:t", "sort order"}},
        {Message::KnownType::ALBUM_MENU_FOR_REMIXER_REQ, 0x1702, "album menu for remixer request", {"r:m:s:t", "sort", "artist ID"}},
        {Message::KnownType::TRACK_MENU_FOR_REMIXER_AND_ALBUM, 0x1802, "track menu for remixer and album request", {"r:m:s:t:", "sort", "artist ID", "album ID, or -1 for ALL"}},
        {Message::KnownType::REKORDBOX_METADATA_REQ, 0x2002, "rekordbox track metadata request", {"r:m:s:t", "rekordbox id"}},
        {Message::KnownType::ALBUM_ART_REQ, 0x2003, "album art request", {"r:m:s:t", "artwork id"}},
        {Message::KnownType::WAVE_PREVIEW_REQ, 0x2004, "track waveform preview request", {"r:m:s:t", "unknown (4)", "rekordbox id", "unknown (0)"}},
        {Message::KnownType::FOLDER_MENU_REQ, 0x2006, "folder menu request", {"r:m:s:t", "sort order?", "folder id (-1 for root)", "unknown (0)"}},
        {Message::KnownType::CUE_LIST_REQ, 0x2104, "track cue list request", {"r:m:s:t", "rekordbox id"}},
        {Message::KnownType::UNANALYZED_METADATA_REQ, 0x2202, "unanalyzed track metadata request", {"r:m:s:t", "track number"}},
        {Message::KnownType::BEAT_GRID_REQ, 0x2204, "beat grid request", {"r:m:s:t", "rekordbox id"}},
        {Message::KnownType::WAVE_DETAIL_REQ, 0x2904, "track waveform detail request", {"r:m:s:t", "rekordbox id"}},
        {Message::KnownType::CUE_LIST_EXT_REQ, 0x2b04, "track extended cue list request", {"r:m:s:t", "rekordbox id", "unknown (0)"}},
        {Message::KnownType::ANLZ_TAG_REQ, 0x2c04, "anlz file tag content request", {"r:m:s:t", "rekordbox id", "tag type", "file extension"}},
        {Message::KnownType::RENDER_MENU_REQ, 0x3000, "render items from last requested menu", {"r:m:s:t", "offset", "limit", "unknown (0)", "len_a (=limit)?", "unknown (0)"}},
        {Message::KnownType::MENU_AVAILABLE, 0x4000, "requested menu is available", {"request type", "# items available"}},
        {Message::KnownType::MENU_HEADER, 0x4001, "rendered menu header", {}},
        {Message::KnownType::ALBUM_ART, 0x4002, "album art", {"request type", "unknown (0)", "image length", "image bytes"}},
        {Message::KnownType::UNAVAILABLE, 0x4003, "requested media unavailable", {"request type"}},
        {Message::KnownType::MENU_ITEM, 0x4101, "rendered menu item", {"numeric 1 (parent id, e.g. artist for track)", "numeric 2 (this id)",
                                                                      "label 1 byte size", "label 1", "label 2 byte size", "label 2",
                                                                      "item type", "flags? byte 3 is 1 when track played", "album art id", "playlist position"}},
        {Message::KnownType::MENU_FOOTER, 0x4201, "rendered menu footer", {}},
        {Message::KnownType::WAVE_PREVIEW, 0x4402, "track waveform preview", {"request type", "unknown (0)", "waveform length", "waveform bytes"}},
        {Message::KnownType::BEAT_GRID, 0x4602, "beat grid", {"request type", "unknown (0)", "beat grid length", "beat grid bytes", "unknown (0)"}},
        {Message::KnownType::CUE_LIST, 0x4702, "memory points, loops, and hot cues", {"request type", "unknown", "blob 1 length", "blob 1",
                                                                                      "unknown (0x24)", "unknown", "unknown", "blob 2 length", "blob 2"}},
        {Message::KnownType::WAVE_DETAIL, 0x4a02, "track waveform detail", {"request type", "unknown (0)", "waveform length", "waveform bytes"}},
        {Message::KnownType::CUE_LIST_EXT, 0x4e02, "extended memory points, loops, and hot cues", {"request type", "unknown (0)",
                                                                                                    "blob length", "blob", "entry count"}},
        {Message::KnownType::ANLZ_TAG, 0x4f02, "anlz file tag content", {"request type", "unknown (0)", "tag length", "tag bytes", "unknown (1)"}}
    };
    return info;
}

const std::unordered_map<uint16_t, Message::KnownType> Message::KNOWN_TYPE_MAP = []() {
    std::unordered_map<uint16_t, Message::KnownType> map;
    for (const auto& entry : knownTypeInfoList()) {
        map.emplace(entry.protocolValue, entry.type);
    }
    return map;
}();

const std::unordered_map<uint32_t, Message::MenuItemType> Message::MENU_ITEM_TYPE_MAP = {
    {0x0001, MenuItemType::FOLDER},
    {0x0002, MenuItemType::ALBUM_TITLE},
    {0x0003, MenuItemType::DISC},
    {0x0004, MenuItemType::TRACK_TITLE},
    {0x0006, MenuItemType::GENRE},
    {0x0007, MenuItemType::ARTIST},
    {0x0008, MenuItemType::PLAYLIST},
    {0x000a, MenuItemType::RATING},
    {0x000b, MenuItemType::DURATION},
    {0x000d, MenuItemType::TEMPO},
    {0x000e, MenuItemType::LABEL},
    {0x000f, MenuItemType::KEY},
    {0x0010, MenuItemType::BIT_RATE},
    {0x0011, MenuItemType::YEAR},
    {0x0013, MenuItemType::COLOR_NONE},
    {0x0014, MenuItemType::COLOR_PINK},
    {0x0015, MenuItemType::COLOR_RED},
    {0x0016, MenuItemType::COLOR_ORANGE},
    {0x0017, MenuItemType::COLOR_YELLOW},
    {0x0018, MenuItemType::COLOR_GREEN},
    {0x0019, MenuItemType::COLOR_AQUA},
    {0x001a, MenuItemType::COLOR_BLUE},
    {0x001b, MenuItemType::COLOR_PURPLE},
    {0x0023, MenuItemType::COMMENT},
    {0x0024, MenuItemType::HISTORY_PLAYLIST},
    {0x0028, MenuItemType::ORIGINAL_ARTIST},
    {0x0029, MenuItemType::REMIXER},
    {0x002e, MenuItemType::DATE_ADDED},
    {0x0080, MenuItemType::GENRE_MENU},
    {0x0081, MenuItemType::ARTIST_MENU},
    {0x0082, MenuItemType::ALBUM_MENU},
    {0x0083, MenuItemType::TRACK_MENU},
    {0x0084, MenuItemType::PLAYLIST_MENU},
    {0x0085, MenuItemType::BPM_MENU},
    {0x0086, MenuItemType::RATING_MENU},
    {0x0087, MenuItemType::YEAR_MENU},
    {0x0088, MenuItemType::REMIXER_MENU},
    {0x0089, MenuItemType::LABEL_MENU},
    {0x008a, MenuItemType::ORIGINAL_ARTIST_MENU},
    {0x008b, MenuItemType::KEY_MENU},
    {0x008c, MenuItemType::DATE_ADDED_MENU},
    {0x008e, MenuItemType::COLOR_MENU},
    {0x0090, MenuItemType::FOLDER_MENU},
    {0x0091, MenuItemType::SEARCH_MENU},
    {0x0092, MenuItemType::TIME_MENU},
    {0x0093, MenuItemType::BIT_RATE_MENU},
    {0x0094, MenuItemType::FILENAME_MENU},
    {0x0095, MenuItemType::HISTORY_MENU},
    {0x0098, MenuItemType::HOT_CUE_BANK_MENU},
    {0x00a0, MenuItemType::ALL},
    {0x0204, MenuItemType::TRACK_TITLE_AND_ALBUM},
    {0x0604, MenuItemType::TRACK_TITLE_AND_GENRE},
    {0x0704, MenuItemType::TRACK_TITLE_AND_ARTIST},
    {0x0a04, MenuItemType::TRACK_TITLE_AND_RATING},
    {0x0b04, MenuItemType::TRACK_TITLE_AND_TIME},
    {0x0d04, MenuItemType::TRACK_TITLE_AND_BPM},
    {0x0e04, MenuItemType::TRACK_TITLE_AND_LABEL},
    {0x0f04, MenuItemType::TRACK_TITLE_AND_KEY},
    {0x1004, MenuItemType::TRACK_TITLE_AND_RATE},
    {0x1a04, MenuItemType::TRACK_LIST_ENTRY_BY_COLOR},
    {0x2304, MenuItemType::TRACK_TITLE_AND_COMMENT},
    {0x2804, MenuItemType::TRACK_TITLE_AND_ORIGINAL_ARTIST},
    {0x2904, MenuItemType::TRACK_TITLE_AND_REMIXER},
    {0x2a04, MenuItemType::TRACK_TITLE_AND_DJ_PLAY_COUNT},
    {0x2e04, MenuItemType::TRACK_TITLE_AND_DATE_ADDED}
};

const std::unordered_map<uint8_t, Message::MenuIdentifier> Message::MENU_IDENTIFIER_MAP = {
    {1, MenuIdentifier::MAIN_MENU},
    {2, MenuIdentifier::SUB_MENU},
    {3, MenuIdentifier::TRACK_INFO},
    {5, MenuIdentifier::SORT_MENU},
    {8, MenuIdentifier::DATA}
};

Message::Message(int64_t transactionValue, int64_t messageTypeValue, std::vector<FieldPtr> args)
    : Message(NumberField(transactionValue, 4), NumberField(messageTypeValue, 2), std::move(args))
{
}

Message::Message(int64_t transactionValue, KnownType type, std::vector<FieldPtr> args)
    : Message(transactionValue, static_cast<int64_t>(static_cast<uint16_t>(type)), std::move(args))
{
}

Message::Message(const NumberField& transactionField, const NumberField& messageTypeField,
                 std::vector<FieldPtr> args)
    : transaction(transactionField)
    , messageType(messageTypeField)
    , argumentCount(NumberField(static_cast<int64_t>(args.size()), 1))
    , arguments(std::move(args))
{
    if (transaction.getSize() != 4) {
        throw std::invalid_argument("Message transaction sequence number must be 4 bytes long");
    }
    if (messageType.getSize() != 2) {
        throw std::invalid_argument("Message type must be 2 bytes long");
    }
    if (arguments.size() > 12) {
        throw std::invalid_argument("Messages cannot have more than 12 arguments");
    }

    auto known = KNOWN_TYPE_MAP.find(static_cast<uint16_t>(messageType.getValue()));
    if (known != KNOWN_TYPE_MAP.end()) {
        knownType = known->second;
    }

    std::vector<uint8_t> argTags(12, 0);
    for (size_t i = 0; i < arguments.size(); ++i) {
        argTags[i] = arguments[i]->getArgumentTag();
    }

    auto argTagField = std::make_shared<BinaryField>(argTags);
    fields.reserve(arguments.size() + 5);
    fields.push_back(std::make_shared<NumberField>(MESSAGE_START));
    fields.push_back(std::make_shared<NumberField>(transaction));
    fields.push_back(std::make_shared<NumberField>(messageType));
    fields.push_back(std::make_shared<NumberField>(argumentCount));
    fields.push_back(argTagField);
    for (const auto& arg : arguments) {
        fields.push_back(arg);
    }
}

Message Message::read(DataReader& reader) {
    auto startField = Field::read(reader);
    auto startNumber = std::dynamic_pointer_cast<NumberField>(startField);
    if (!startNumber) {
        throw std::runtime_error("Did not find number field reading start of message");
    }
    if (startNumber->getSize() != 4) {
        throw std::runtime_error("Number field to start message must be of size 4");
    }
    if (startNumber->getValue() != MESSAGE_START.getValue()) {
        throw std::runtime_error("Number field had wrong value to start message");
    }

    auto transactionField = Field::read(reader);
    auto transactionNumber = std::dynamic_pointer_cast<NumberField>(transactionField);
    if (!transactionNumber || transactionNumber->getSize() != 4) {
        throw std::runtime_error("Did not find number field reading transaction ID of message");
    }

    auto typeField = Field::read(reader);
    auto typeNumber = std::dynamic_pointer_cast<NumberField>(typeField);
    if (!typeNumber || typeNumber->getSize() != 2) {
        throw std::runtime_error("Did not find number field reading type of message");
    }

    auto argCountField = Field::read(reader);
    auto argCountNumber = std::dynamic_pointer_cast<NumberField>(argCountField);
    if (!argCountNumber || argCountNumber->getSize() != 1) {
        throw std::runtime_error("Did not find number field reading argument count of message");
    }

    const int argCount = static_cast<int>(argCountNumber->getValue());
    if (argCount < 0 || argCount > 12) {
        throw std::runtime_error("Illegal argument count while reading message");
    }

    auto argTypesField = Field::read(reader);
    auto argTypesBinary = std::dynamic_pointer_cast<BinaryField>(argTypesField);
    if (!argTypesBinary) {
        throw std::runtime_error("Did not find binary field reading argument types of message");
    }

    std::vector<uint8_t> argTags;
    auto argTagValue = argTypesBinary->getValue();
    argTags.assign(argTagValue.begin(), argTagValue.end());
    if (static_cast<int>(argTags.size()) < argCount) {
        throw std::runtime_error("Argument tag list shorter than argument count");
    }

    std::vector<FieldPtr> args;
    args.reserve(argCount);
    FieldPtr lastArg;
    for (int i = 0; i < argCount; ++i) {
        if (argTags[i] == 0x03 && lastArg) {
            auto lastNumber = std::dynamic_pointer_cast<NumberField>(lastArg);
            if (lastNumber && lastNumber->getValue() == 0) {
                auto emptyBinary = std::make_shared<BinaryField>(std::vector<uint8_t>{});
                args.push_back(emptyBinary);
                lastArg = emptyBinary;
                continue;
            }
        }
        auto arg = Field::read(reader);
        if (arg->getArgumentTag() != argTags[i]) {
            throw std::runtime_error("Found argument of wrong type reading message");
        }
        args.push_back(arg);
        lastArg = arg;
    }

    return Message(*transactionNumber, *typeNumber, std::move(args));
}

std::string Message::toString() const {
    std::ostringstream result;
    result << "Message: [transaction: " << transaction.getValue();
    result << fmt::format(", type: 0x{:04x} (", messageType.getValue());
    if (knownType) {
        const auto* info = getKnownTypeInfo(*knownType);
        if (info) {
            result << info->description;
        } else {
            result << "unknown";
        }
    } else {
        result << "unknown";
    }
    result << "), arg count: " << argumentCount.getValue() << ", arguments:\n";

    for (size_t i = 0; i < arguments.size(); ++i) {
        const auto& arg = arguments[i];
        result << fmt::format("{:4d}: ", static_cast<int>(i + 1));
        if (auto num = std::dynamic_pointer_cast<NumberField>(arg)) {
            const auto value = num->getValue();
            result << fmt::format("number: {:10d} (0x{:08x})", value, value);
        } else if (auto bin = std::dynamic_pointer_cast<BinaryField>(arg)) {
            result << fmt::format("blob length {}:", bin->getSize());
            auto bytes = bin->getValue();
            for (auto b : bytes) {
                result << fmt::format(" {:02x}", b);
            }
        } else if (auto str = std::dynamic_pointer_cast<StringField>(arg)) {
            result << fmt::format("string length {}: \"{}\"", str->getSize(), str->getValue());
        } else {
            result << "unknown: " << arg->toString();
        }

        std::string argDescription = "unknown";
        if (knownType) {
            const auto* info = getKnownTypeInfo(*knownType);
            if (info && i < info->arguments.size()) {
                argDescription = info->arguments[i];
            }
            if (*knownType == KnownType::MENU_ITEM && i == 6) {
                if (auto num = std::dynamic_pointer_cast<NumberField>(arg)) {
                    auto match = MENU_ITEM_TYPE_MAP.find(static_cast<uint32_t>(num->getValue() & 0xffff));
                    if (match != MENU_ITEM_TYPE_MAP.end()) {
                        argDescription += ": " + getMenuItemTypeName(match->second);
                    }
                }
            }
        }
        result << fmt::format(" [{}]\n", argDescription);
    }
    result << "]";
    return result.str();
}

long Message::getMenuResultsCount() const {
    if (!knownType || *knownType != KnownType::MENU_AVAILABLE) {
        throw std::invalid_argument("getMenuResultsCount() can only be used with MENU_AVAILABLE responses.");
    }
    auto countField = std::dynamic_pointer_cast<NumberField>(arguments.at(1));
    if (!countField) {
        throw std::runtime_error("MENU_AVAILABLE response does not contain a numeric count");
    }
    return countField->getValue();
}

Message::MenuItemType Message::getMenuItemType() const {
    if (!knownType || *knownType != KnownType::MENU_ITEM) {
        throw std::invalid_argument("getMenuItemType() can only be used with MENU_ITEM responses.");
    }
    auto typeField = std::dynamic_pointer_cast<NumberField>(arguments.at(6));
    if (!typeField) {
        throw std::runtime_error("MENU_ITEM does not contain numeric type field");
    }
    auto match = MENU_ITEM_TYPE_MAP.find(static_cast<uint32_t>(typeField->getValue() & 0xffff));
    if (match == MENU_ITEM_TYPE_MAP.end()) {
        return MenuItemType::UNKNOWN;
    }
    return match->second;
}

void Message::write(asio::ip::tcp::socket& socket) const {
    for (const auto& field : fields) {
        field->write(socket);
    }
}

const Message::KnownTypeInfo* Message::getKnownTypeInfo(KnownType type) {
    for (const auto& entry : knownTypeInfoList()) {
        if (entry.type == type) {
            return &entry;
        }
    }
    return nullptr;
}

std::string Message::getMenuItemTypeName(MenuItemType type) {
    switch (type) {
        case MenuItemType::FOLDER: return "FOLDER";
        case MenuItemType::ALBUM_TITLE: return "ALBUM_TITLE";
        case MenuItemType::DISC: return "DISC";
        case MenuItemType::TRACK_TITLE: return "TRACK_TITLE";
        case MenuItemType::GENRE: return "GENRE";
        case MenuItemType::ARTIST: return "ARTIST";
        case MenuItemType::PLAYLIST: return "PLAYLIST";
        case MenuItemType::RATING: return "RATING";
        case MenuItemType::DURATION: return "DURATION";
        case MenuItemType::TEMPO: return "TEMPO";
        case MenuItemType::LABEL: return "LABEL";
        case MenuItemType::KEY: return "KEY";
        case MenuItemType::BIT_RATE: return "BIT_RATE";
        case MenuItemType::YEAR: return "YEAR";
        case MenuItemType::COLOR_NONE: return "COLOR_NONE";
        case MenuItemType::COLOR_PINK: return "COLOR_PINK";
        case MenuItemType::COLOR_RED: return "COLOR_RED";
        case MenuItemType::COLOR_ORANGE: return "COLOR_ORANGE";
        case MenuItemType::COLOR_YELLOW: return "COLOR_YELLOW";
        case MenuItemType::COLOR_GREEN: return "COLOR_GREEN";
        case MenuItemType::COLOR_AQUA: return "COLOR_AQUA";
        case MenuItemType::COLOR_BLUE: return "COLOR_BLUE";
        case MenuItemType::COLOR_PURPLE: return "COLOR_PURPLE";
        case MenuItemType::COMMENT: return "COMMENT";
        case MenuItemType::HISTORY_PLAYLIST: return "HISTORY_PLAYLIST";
        case MenuItemType::ORIGINAL_ARTIST: return "ORIGINAL_ARTIST";
        case MenuItemType::REMIXER: return "REMIXER";
        case MenuItemType::DATE_ADDED: return "DATE_ADDED";
        case MenuItemType::GENRE_MENU: return "GENRE_MENU";
        case MenuItemType::ARTIST_MENU: return "ARTIST_MENU";
        case MenuItemType::ALBUM_MENU: return "ALBUM_MENU";
        case MenuItemType::TRACK_MENU: return "TRACK_MENU";
        case MenuItemType::PLAYLIST_MENU: return "PLAYLIST_MENU";
        case MenuItemType::BPM_MENU: return "BPM_MENU";
        case MenuItemType::RATING_MENU: return "RATING_MENU";
        case MenuItemType::YEAR_MENU: return "YEAR_MENU";
        case MenuItemType::REMIXER_MENU: return "REMIXER_MENU";
        case MenuItemType::LABEL_MENU: return "LABEL_MENU";
        case MenuItemType::ORIGINAL_ARTIST_MENU: return "ORIGINAL_ARTIST_MENU";
        case MenuItemType::KEY_MENU: return "KEY_MENU";
        case MenuItemType::DATE_ADDED_MENU: return "DATE_ADDED_MENU";
        case MenuItemType::COLOR_MENU: return "COLOR_MENU";
        case MenuItemType::FOLDER_MENU: return "FOLDER_MENU";
        case MenuItemType::SEARCH_MENU: return "SEARCH_MENU";
        case MenuItemType::TIME_MENU: return "TIME_MENU";
        case MenuItemType::BIT_RATE_MENU: return "BIT_RATE_MENU";
        case MenuItemType::FILENAME_MENU: return "FILENAME_MENU";
        case MenuItemType::HISTORY_MENU: return "HISTORY_MENU";
        case MenuItemType::HOT_CUE_BANK_MENU: return "HOT_CUE_BANK_MENU";
        case MenuItemType::ALL: return "ALL";
        case MenuItemType::TRACK_TITLE_AND_ALBUM: return "TRACK_TITLE_AND_ALBUM";
        case MenuItemType::TRACK_TITLE_AND_GENRE: return "TRACK_TITLE_AND_GENRE";
        case MenuItemType::TRACK_TITLE_AND_ARTIST: return "TRACK_TITLE_AND_ARTIST";
        case MenuItemType::TRACK_TITLE_AND_RATING: return "TRACK_TITLE_AND_RATING";
        case MenuItemType::TRACK_TITLE_AND_TIME: return "TRACK_TITLE_AND_TIME";
        case MenuItemType::TRACK_TITLE_AND_BPM: return "TRACK_TITLE_AND_BPM";
        case MenuItemType::TRACK_TITLE_AND_LABEL: return "TRACK_TITLE_AND_LABEL";
        case MenuItemType::TRACK_TITLE_AND_KEY: return "TRACK_TITLE_AND_KEY";
        case MenuItemType::TRACK_TITLE_AND_RATE: return "TRACK_TITLE_AND_RATE";
        case MenuItemType::TRACK_LIST_ENTRY_BY_COLOR: return "TRACK_LIST_ENTRY_BY_COLOR";
        case MenuItemType::TRACK_TITLE_AND_COMMENT: return "TRACK_TITLE_AND_COMMENT";
        case MenuItemType::TRACK_TITLE_AND_ORIGINAL_ARTIST: return "TRACK_TITLE_AND_ORIGINAL_ARTIST";
        case MenuItemType::TRACK_TITLE_AND_REMIXER: return "TRACK_TITLE_AND_REMIXER";
        case MenuItemType::TRACK_TITLE_AND_DJ_PLAY_COUNT: return "TRACK_TITLE_AND_DJ_PLAY_COUNT";
        case MenuItemType::TRACK_TITLE_AND_DATE_ADDED: return "TRACK_TITLE_AND_DATE_ADDED";
        case MenuItemType::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

} // namespace beatlink::dbserver
