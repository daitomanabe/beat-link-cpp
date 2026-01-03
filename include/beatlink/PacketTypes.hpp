#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>

namespace beatlink {

/**
 * Library version information.
 * Defined here (not in BeatLink.hpp) to avoid circular dependencies with ApiSchema.hpp.
 */
struct Version {
    static constexpr int MAJOR = 0;
    static constexpr int MINOR = 2;
    static constexpr int PATCH = 0;
    static constexpr const char* STRING = "0.2.0";
};

/**
 * Network ports used in the DJ Link protocol.
 */
struct Ports {
    static constexpr uint16_t ANNOUNCEMENT = 50000;  // Device discovery & negotiation
    static constexpr uint16_t BEAT = 50001;          // Beat packets
    static constexpr uint16_t UPDATE = 50002;        // CDJ/Mixer status updates
};

/**
 * Known packet types used in the DJ Link protocol.
 * Ported from org.deepsymmetry.beatlink.Util.PacketType
 */
enum class PacketType : uint8_t {
    // Port 50001 (BEAT_PORT) packets
    FADER_START_COMMAND = 0x02,     // Mixer tells players to start/stop
    CHANNELS_ON_AIR = 0x03,         // Mixer tells which channels are on/off air
    PRECISE_POSITION = 0x0b,        // CDJ-3000+ position reports
    MASTER_HANDOFF_REQUEST = 0x26,  // Request to relinquish tempo master
    MASTER_HANDOFF_RESPONSE = 0x27, // Response to master handoff request
    BEAT = 0x28,                    // Beat announcement
    SYNC_CONTROL = 0x2a,            // Sync on/off command

    // Port 50000 (ANNOUNCEMENT_PORT) packets
    DEVICE_NUMBER_STAGE_1 = 0x00,   // First stage of device number claim
    DEVICE_NUMBER_WILL_ASSIGN = 0x01, // Mixer will assign device number
    DEVICE_NUMBER_STAGE_2 = 0x02,   // Second stage of device number claim
    DEVICE_NUMBER_ASSIGN = 0x03,    // Mixer assigns device number
    DEVICE_NUMBER_STAGE_3 = 0x04,   // Third stage of device number claim
    DEVICE_NUMBER_ASSIGNMENT_FINISHED = 0x05, // Assignment complete
    DEVICE_KEEP_ALIVE = 0x06,       // Device keep-alive
    DEVICE_NUMBER_IN_USE = 0x08,    // Device number defense
    DEVICE_HELLO = 0x0a,            // Device announcement

    // Port 50002 (UPDATE_PORT) packets
    MEDIA_QUERY = 0x05,             // Ask player about mounted media
    MEDIA_RESPONSE = 0x06,          // Response to media query
    CDJ_STATUS = 0x0a,              // Detailed CDJ status
    DEVICE_REKORDBOX_LIGHTING_HELLO = 0x10, // Opus Quad lighting hello
    LOAD_TRACK_COMMAND = 0x19,      // Load track command
    LOAD_TRACK_ACK = 0x1a,          // Load track acknowledgment
    MIXER_STATUS = 0x29,            // Mixer status update
    LOAD_SETTINGS_COMMAND = 0x34,   // Apply settings command
    OPUS_METADATA = 0x56,           // Opus Quad metadata
};

/**
 * Helper class for packet type operations.
 */
class PacketTypes {
public:
    struct PacketInfo {
        PacketType type;
        std::string name;
        uint16_t port;
    };

    /**
     * Get information about a packet type.
     */
    static std::optional<PacketInfo> getInfo(PacketType type) {
        static const std::unordered_map<PacketType, PacketInfo> info = {
            // Beat port packets
            {PacketType::FADER_START_COMMAND, {PacketType::FADER_START_COMMAND, "Fader Start", Ports::BEAT}},
            {PacketType::CHANNELS_ON_AIR, {PacketType::CHANNELS_ON_AIR, "Channels On Air", Ports::BEAT}},
            {PacketType::PRECISE_POSITION, {PacketType::PRECISE_POSITION, "Precise Position", Ports::BEAT}},
            {PacketType::MASTER_HANDOFF_REQUEST, {PacketType::MASTER_HANDOFF_REQUEST, "Master Handoff Request", Ports::BEAT}},
            {PacketType::MASTER_HANDOFF_RESPONSE, {PacketType::MASTER_HANDOFF_RESPONSE, "Master Handoff Response", Ports::BEAT}},
            {PacketType::BEAT, {PacketType::BEAT, "Beat", Ports::BEAT}},
            {PacketType::SYNC_CONTROL, {PacketType::SYNC_CONTROL, "Sync Control", Ports::BEAT}},

            // Announcement port packets
            {PacketType::DEVICE_NUMBER_STAGE_1, {PacketType::DEVICE_NUMBER_STAGE_1, "Device Number Claim Stage 1", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_NUMBER_WILL_ASSIGN, {PacketType::DEVICE_NUMBER_WILL_ASSIGN, "Device Number Will Be Assigned", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_NUMBER_STAGE_2, {PacketType::DEVICE_NUMBER_STAGE_2, "Device Number Claim Stage 2", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_NUMBER_ASSIGN, {PacketType::DEVICE_NUMBER_ASSIGN, "Device Number Assignment", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_NUMBER_STAGE_3, {PacketType::DEVICE_NUMBER_STAGE_3, "Device Number Claim Stage 3", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_NUMBER_ASSIGNMENT_FINISHED, {PacketType::DEVICE_NUMBER_ASSIGNMENT_FINISHED, "Device Number Assignment Finished", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_KEEP_ALIVE, {PacketType::DEVICE_KEEP_ALIVE, "Device Keep-Alive", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_NUMBER_IN_USE, {PacketType::DEVICE_NUMBER_IN_USE, "Device Number In Use", Ports::ANNOUNCEMENT}},
            {PacketType::DEVICE_HELLO, {PacketType::DEVICE_HELLO, "Device Hello", Ports::ANNOUNCEMENT}},

            // Update port packets
            {PacketType::MEDIA_QUERY, {PacketType::MEDIA_QUERY, "Media Query", Ports::UPDATE}},
            {PacketType::MEDIA_RESPONSE, {PacketType::MEDIA_RESPONSE, "Media Response", Ports::UPDATE}},
            {PacketType::CDJ_STATUS, {PacketType::CDJ_STATUS, "CDJ Status", Ports::UPDATE}},
            {PacketType::DEVICE_REKORDBOX_LIGHTING_HELLO, {PacketType::DEVICE_REKORDBOX_LIGHTING_HELLO, "Rekordbox Lighting Hello", Ports::UPDATE}},
            {PacketType::LOAD_TRACK_COMMAND, {PacketType::LOAD_TRACK_COMMAND, "Load Track Command", Ports::UPDATE}},
            {PacketType::LOAD_TRACK_ACK, {PacketType::LOAD_TRACK_ACK, "Load Track Acknowledgment", Ports::UPDATE}},
            {PacketType::MIXER_STATUS, {PacketType::MIXER_STATUS, "Mixer Status", Ports::UPDATE}},
            {PacketType::LOAD_SETTINGS_COMMAND, {PacketType::LOAD_SETTINGS_COMMAND, "Load Settings Command", Ports::UPDATE}},
            {PacketType::OPUS_METADATA, {PacketType::OPUS_METADATA, "OPUS Metadata", Ports::UPDATE}},
        };

        auto it = info.find(type);
        if (it != info.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * Look up a packet type by port and protocol value.
     */
    static std::optional<PacketType> lookup(uint16_t port, uint8_t protocolValue) {
        static bool initialized = false;
        static std::unordered_map<uint16_t, std::unordered_map<uint8_t, PacketType>> portMap;

        if (!initialized) {
            // Build the port map from packet info
            for (int i = 0; i <= 0xff; ++i) {
                auto pt = static_cast<PacketType>(i);
                auto info = getInfo(pt);
                if (info) {
                    portMap[info->port][static_cast<uint8_t>(pt)] = pt;
                }
            }
            initialized = true;
        }

        auto portIt = portMap.find(port);
        if (portIt == portMap.end()) {
            return std::nullopt;
        }

        auto typeIt = portIt->second.find(protocolValue);
        if (typeIt == portIt->second.end()) {
            return std::nullopt;
        }

        return typeIt->second;
    }
};

} // namespace beatlink
