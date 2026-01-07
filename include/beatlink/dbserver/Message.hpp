#pragma once

#include "BinaryField.hpp"
#include "Field.hpp"
#include "NumberField.hpp"
#include "StringField.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace beatlink::dbserver {

class DataReader;

class Message {
public:
    static const NumberField MESSAGE_START;

    enum class KnownType : uint16_t {
        SETUP_REQ = 0x0000,
        INVALID_DATA = 0x0001,
        TEARDOWN_REQ = 0x0100,
        ROOT_MENU_REQ = 0x1000,
        GENRE_MENU_REQ = 0x1001,
        ARTIST_MENU_REQ = 0x1002,
        ALBUM_MENU_REQ = 0x1003,
        TRACK_MENU_REQ = 0x1004,
        BPM_MENU_REQ = 0x1006,
        RATING_MENU_REQ = 0x1007,
        YEAR_MENU_REQ = 0x1008,
        LABEL_MENU_REQ = 0x100a,
        COLOR_MENU_REQ = 0x100d,
        TIME_MENU_REQ = 0x1010,
        BIT_RATE_MENU_REQ = 0x1011,
        HISTORY_MENU_REQ = 0x1012,
        FILENAME_MENU_REQ = 0x1013,
        KEY_MENU_REQ = 0x1014,
        ARTIST_MENU_FOR_GENRE_REQ = 0x1101,
        ALBUM_MENU_FOR_ARTIST_REQ = 0x1102,
        TRACK_MENU_FOR_ALBUM_REQ = 0x1103,
        PLAYLIST_REQ = 0x1105,
        BPM_RANGE_REQ = 0x1106,
        TRACK_MENU_FOR_RATING_REQ = 0x1107,
        YEAR_MENU_FOR_DECADE_REQ = 0x1108,
        ARTIST_MENU_FOR_LABEL_REQ = 0x110a,
        TRACK_MENU_FOR_COLOR_REQ = 0x110d,
        TRACK_MENU_FOR_TIME_REQ = 0x1110,
        TRACK_MENU_FOR_BIT_RATE_REQ = 0x1111,
        TRACK_MENU_FOR_HISTORY_REQ = 0x1112,
        NEIGHBOR_MENU_FOR_KEY = 0x1114,
        ALBUM_MENU_FOR_GENRE_AND_ARTIST = 0x1201,
        TRACK_MENU_FOR_ARTIST_AND_ALBUM = 0x1202,
        TRACK_MENU_FOR_BPM_AND_DISTANCE = 0x1206,
        TRACK_MENU_FOR_DECADE_YEAR_REQ = 0x1208,
        ALBUM_MENU_FOR_LABEL_AND_ARTIST = 0x120a,
        TRACK_MENU_FOR_KEY_AND_DISTANCE = 0x1214,
        SEARCH_MENU = 0x1300,
        TRACK_MENU_FOR_GENRE_ARTIST_AND_ALBUM = 0x1301,
        ORIGINAL_ARTIST_MENU_REQ = 0x1302,
        TRACK_MENU_FOR_LABEL_ARTIST_AND_ALBUM = 0x130a,
        ALBUM_MENU_FOR_ORIGINAL_ARTIST_REQ = 0x1402,
        TRACK_MENU_FOR_ORIGINAL_ARTIST_AND_ALBUM = 0x1502,
        REMIXER_MENU_REQ = 0x1602,
        ALBUM_MENU_FOR_REMIXER_REQ = 0x1702,
        TRACK_MENU_FOR_REMIXER_AND_ALBUM = 0x1802,
        REKORDBOX_METADATA_REQ = 0x2002,
        ALBUM_ART_REQ = 0x2003,
        WAVE_PREVIEW_REQ = 0x2004,
        FOLDER_MENU_REQ = 0x2006,
        CUE_LIST_REQ = 0x2104,
        UNANALYZED_METADATA_REQ = 0x2202,
        BEAT_GRID_REQ = 0x2204,
        WAVE_DETAIL_REQ = 0x2904,
        CUE_LIST_EXT_REQ = 0x2b04,
        ANLZ_TAG_REQ = 0x2c04,
        RENDER_MENU_REQ = 0x3000,
        MENU_AVAILABLE = 0x4000,
        MENU_HEADER = 0x4001,
        ALBUM_ART = 0x4002,
        UNAVAILABLE = 0x4003,
        MENU_ITEM = 0x4101,
        MENU_FOOTER = 0x4201,
        WAVE_PREVIEW = 0x4402,
        BEAT_GRID = 0x4602,
        CUE_LIST = 0x4702,
        WAVE_DETAIL = 0x4a02,
        CUE_LIST_EXT = 0x4e02,
        ANLZ_TAG = 0x4f02
    };

    enum class MenuItemType : uint32_t {
        FOLDER = 0x0001,
        ALBUM_TITLE = 0x0002,
        DISC = 0x0003,
        TRACK_TITLE = 0x0004,
        GENRE = 0x0006,
        ARTIST = 0x0007,
        PLAYLIST = 0x0008,
        RATING = 0x000a,
        DURATION = 0x000b,
        TEMPO = 0x000d,
        LABEL = 0x000e,
        KEY = 0x000f,
        BIT_RATE = 0x0010,
        YEAR = 0x0011,
        COLOR_NONE = 0x0013,
        COLOR_PINK = 0x0014,
        COLOR_RED = 0x0015,
        COLOR_ORANGE = 0x0016,
        COLOR_YELLOW = 0x0017,
        COLOR_GREEN = 0x0018,
        COLOR_AQUA = 0x0019,
        COLOR_BLUE = 0x001a,
        COLOR_PURPLE = 0x001b,
        COMMENT = 0x0023,
        HISTORY_PLAYLIST = 0x0024,
        ORIGINAL_ARTIST = 0x0028,
        REMIXER = 0x0029,
        DATE_ADDED = 0x002e,
        GENRE_MENU = 0x0080,
        ARTIST_MENU = 0x0081,
        ALBUM_MENU = 0x0082,
        TRACK_MENU = 0x0083,
        PLAYLIST_MENU = 0x0084,
        BPM_MENU = 0x0085,
        RATING_MENU = 0x0086,
        YEAR_MENU = 0x0087,
        REMIXER_MENU = 0x0088,
        LABEL_MENU = 0x0089,
        ORIGINAL_ARTIST_MENU = 0x008a,
        KEY_MENU = 0x008b,
        DATE_ADDED_MENU = 0x008c,
        COLOR_MENU = 0x008e,
        FOLDER_MENU = 0x0090,
        SEARCH_MENU = 0x0091,
        TIME_MENU = 0x0092,
        BIT_RATE_MENU = 0x0093,
        FILENAME_MENU = 0x0094,
        HISTORY_MENU = 0x0095,
        HOT_CUE_BANK_MENU = 0x0098,
        ALL = 0x00a0,
        TRACK_TITLE_AND_ALBUM = 0x0204,
        TRACK_TITLE_AND_GENRE = 0x0604,
        TRACK_TITLE_AND_ARTIST = 0x0704,
        TRACK_TITLE_AND_RATING = 0x0a04,
        TRACK_TITLE_AND_TIME = 0x0b04,
        TRACK_TITLE_AND_BPM = 0x0d04,
        TRACK_TITLE_AND_LABEL = 0x0e04,
        TRACK_TITLE_AND_KEY = 0x0f04,
        TRACK_TITLE_AND_RATE = 0x1004,
        TRACK_LIST_ENTRY_BY_COLOR = 0x1a04,
        TRACK_TITLE_AND_COMMENT = 0x2304,
        TRACK_TITLE_AND_ORIGINAL_ARTIST = 0x2804,
        TRACK_TITLE_AND_REMIXER = 0x2904,
        TRACK_TITLE_AND_DJ_PLAY_COUNT = 0x2a04,
        TRACK_TITLE_AND_DATE_ADDED = 0x2e04,
        UNKNOWN = 0xffffffff
    };

    enum class MenuIdentifier : uint8_t {
        MAIN_MENU = 1,
        SUB_MENU = 2,
        TRACK_INFO = 3,
        SORT_MENU = 5,
        DATA = 8
    };

    struct KnownTypeInfo {
        KnownType type;
        uint16_t protocolValue;
        std::string description;
        std::vector<std::string> arguments;
    };

    static const std::unordered_map<uint16_t, KnownType> KNOWN_TYPE_MAP;
    static const std::unordered_map<uint32_t, MenuItemType> MENU_ITEM_TYPE_MAP;
    static const std::unordered_map<uint8_t, MenuIdentifier> MENU_IDENTIFIER_MAP;

    static constexpr int ALNZ_FILE_TYPE_DAT = 0x00544144;
    static constexpr int ALNZ_FILE_TYPE_EXT = 0x00545845;
    static constexpr int ALNZ_FILE_TYPE_2EX = 0x00584532;
    static constexpr int ANLZ_FILE_TAG_COLOR_WAVEFORM_PREVIEW = 0x34565750;
    static constexpr int ANLZ_FILE_TAG_COLOR_WAVEFORM_DETAIL = 0x35565750;
    static constexpr int ANLZ_FILE_TAG_3BAND_WAVEFORM_PREVIEW = 0x36565750;
    static constexpr int ANLZ_FILE_TAG_3BAND_WAVEFORM_DETAIL = 0x37565750;
    static constexpr int ANLZ_FILE_TAG_SONG_STRUCTURE = 0x49535350;
    static constexpr int ANLZ_FILE_TAG_CUE_COMMENT = 0x324f4350;
    static constexpr int64_t NO_MENU_RESULTS_AVAILABLE = 0xffffffffLL;

    NumberField transaction;
    NumberField messageType;
    std::optional<KnownType> knownType;
    NumberField argumentCount;
    std::vector<FieldPtr> arguments;
    std::vector<FieldPtr> fields;

    Message(int64_t transaction, int64_t messageType, std::vector<FieldPtr> arguments = {});
    Message(int64_t transaction, KnownType messageType, std::vector<FieldPtr> arguments = {});
    Message(const NumberField& transaction, const NumberField& messageType, std::vector<FieldPtr> arguments = {});

    static Message read(DataReader& reader);

    std::string toString() const;

    long getMenuResultsCount() const;
    MenuItemType getMenuItemType() const;

    void write(asio::ip::tcp::socket& socket) const;

    static const KnownTypeInfo* getKnownTypeInfo(KnownType type);
    static std::string getMenuItemTypeName(MenuItemType type);
};

} // namespace beatlink::dbserver
