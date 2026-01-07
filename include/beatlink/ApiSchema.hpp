#pragma once

/**
 * API Introspection / Self-Description System
 *
 * This module enables AI agents to discover the Beat Link API
 * without reading source code. When queried with {"cmd": "describe_api"}
 * or CLI --schema, it returns a JSON schema of all available operations.
 */

#include <string>
#include <vector>
#include <sstream>
#include "PacketTypes.hpp"

namespace beatlink {

/**
 * Parameter metadata for API introspection.
 */
struct ParamInfo {
    std::string name;
    std::string type;          // "int", "float", "string", "bool"
    std::string description;
    std::string unit;          // "bpm", "ms", "percent", etc.
    double min_value = 0.0;
    double max_value = 0.0;
    bool has_range = false;

    [[nodiscard]] std::string toJson() const {
        std::ostringstream oss;
        oss << R"({"name":")" << name
            << R"(","type":")" << type
            << R"(","description":")" << description << "\"";
        if (!unit.empty()) {
            oss << R"(,"unit":")" << unit << "\"";
        }
        if (has_range) {
            oss << R"(,"min":)" << min_value << R"(,"max":)" << max_value;
        }
        oss << "}";
        return oss.str();
    }
};

/**
 * Command metadata for API introspection.
 */
struct CommandInfo {
    std::string name;
    std::string description;
    std::vector<ParamInfo> params;
    std::string returns;       // Return type description

    [[nodiscard]] std::string toJson() const {
        std::string params_json = "[";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) params_json += ",";
            params_json += params[i].toJson();
        }
        params_json += "]";

        std::ostringstream oss;
        oss << R"({"name":")" << name
            << R"(","description":")" << description
            << R"(","params":)" << params_json
            << R"(,"returns":")" << returns << "\"}";
        return oss.str();
    }
};

/**
 * Input/Output format metadata.
 */
struct IoInfo {
    std::string name;
    std::string description;
    std::string format;        // "json", "binary", "osc", etc.
    std::string shape;         // For tensor data: "[N, 4]" etc.

    [[nodiscard]] std::string toJson() const {
        std::ostringstream oss;
        oss << R"({"name":")" << name
            << R"(","description":")" << description
            << R"(","format":")" << format << "\"";
        if (!shape.empty()) {
            oss << R"(,"shape":")" << shape << "\"";
        }
        oss << "}";
        return oss.str();
    }
};

/**
 * Complete API Schema for self-description.
 */
class ApiSchema {
public:
    std::string name;
    std::string version;
    std::string description;
    std::vector<CommandInfo> commands;
    std::vector<IoInfo> inputs;
    std::vector<IoInfo> outputs;

    /**
     * Generate the complete API schema as JSON.
     * This is what gets returned for {"cmd": "describe_api"} or --schema.
     */
    [[nodiscard]] std::string toJson() const {
        std::string commands_json = "[";
        for (size_t i = 0; i < commands.size(); ++i) {
            if (i > 0) commands_json += ",";
            commands_json += commands[i].toJson();
        }
        commands_json += "]";

        std::string inputs_json = "[";
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (i > 0) inputs_json += ",";
            inputs_json += inputs[i].toJson();
        }
        inputs_json += "]";

        std::string outputs_json = "[";
        for (size_t i = 0; i < outputs.size(); ++i) {
            if (i > 0) outputs_json += ",";
            outputs_json += outputs[i].toJson();
        }
        outputs_json += "]";

        std::ostringstream oss;
        oss << R"({"name":")" << name
            << R"(","version":")" << version
            << R"(","description":")" << description
            << R"(","commands":)" << commands_json
            << R"(,"inputs":)" << inputs_json
            << R"(,"outputs":)" << outputs_json << "}";
        return oss.str();
    }
};

/**
 * Get the Beat Link API schema for introspection.
 * AI agents can call this to understand how to use the library.
 */
[[nodiscard]] inline ApiSchema describe_api() {
    ApiSchema schema;
    schema.name = "beatlink";
    schema.version = Version::STRING;
    schema.description = "Pioneer DJ Link protocol library for C++. "
                         "Discovers DJ devices on the network via UDP and receives beat/tempo information.";

    // Commands
    schema.commands = {
        {
            "start_device_finder",
            "Start discovering DJ Link devices on the network (UDP port 50000)",
            {},
            "bool (success)"
        },
        {
            "stop_device_finder",
            "Stop the device discovery process",
            {},
            "void"
        },
        {
            "start_beat_finder",
            "Start receiving beat packets from DJ devices (UDP port 50001)",
            {},
            "bool (success)"
        },
        {
            "stop_beat_finder",
            "Stop receiving beat packets",
            {},
            "void"
        },
        {
            "start_virtual_cdj",
            "Start receiving device status updates (UDP port 50002)",
            {},
            "bool (success)"
        },
        {
            "stop_virtual_cdj",
            "Stop receiving device status updates",
            {},
            "void"
        },
        {
            "start_metadata_finder",
            "Start track metadata lookup",
            {},
            "bool (running)"
        },
        {
            "stop_metadata_finder",
            "Stop track metadata lookup",
            {},
            "void"
        },
        {
            "start_beat_grid_finder",
            "Start beat grid lookup",
            {},
            "bool (running)"
        },
        {
            "stop_beat_grid_finder",
            "Stop beat grid lookup",
            {},
            "void"
        },
        {
            "start_waveform_finder",
            "Start waveform lookup",
            {},
            "bool (running)"
        },
        {
            "stop_waveform_finder",
            "Stop waveform lookup",
            {},
            "void"
        },
        {
            "start_signature_finder",
            "Start time signature analysis",
            {},
            "bool (running)"
        },
        {
            "stop_signature_finder",
            "Stop time signature analysis",
            {},
            "void"
        },
        {
            "start_time_finder",
            "Start track time interpolation",
            {},
            "bool (running)"
        },
        {
            "stop_time_finder",
            "Stop track time interpolation",
            {},
            "void"
        },
        {
            "get_devices",
            "Get list of currently discovered DJ devices",
            {},
            "array of DeviceInfo"
        },
        {
            "get_track_metadata",
            "Get latest track metadata for a player",
            {
                {"player", "int", "Player number (1-4)", "", 0, 0, false}
            },
            "TrackMetadata or null"
        },
        {
            "get_track_position",
            "Get latest track position for a player",
            {
                {"player", "int", "Player number (1-4)", "", 0, 0, false}
            },
            "TrackPosition or null"
        },
        {
            "get_track_time",
            "Get interpolated track time for a player",
            {
                {"player", "int", "Player number (1-4)", "", 0, 0, false}
            },
            "int64 (ms)"
        },
        {
            "get_signature",
            "Get latest time signature for a player",
            {
                {"player", "int", "Player number (1-4)", "", 0, 0, false}
            },
            "string"
        },
        {
            "add_beat_listener",
            "Register a callback for beat events",
            {
                {"callback", "function", "Function called on each beat", "", 0, 0, false}
            },
            "listener_id (int)"
        },
        {
            "remove_beat_listener",
            "Remove a registered beat listener",
            {
                {"listener_id", "int", "ID returned from add_beat_listener", "", 0, 0, false}
            },
            "bool (success)"
        },
        {
            "add_track_metadata_listener",
            "Register a callback for track metadata updates",
            {
                {"callback", "function", "Function called on metadata changes", "", 0, 0, false}
            },
            "listener_id (int)"
        },
        {
            "add_track_position_listener",
            "Register a callback for track position updates",
            {
                {"player", "int", "Player number (1-4)", "", 0, 0, false},
                {"callback", "function", "Function called on position changes", "", 0, 0, false}
            },
            "listener_id (int)"
        },
        {
            "add_signature_listener",
            "Register a callback for signature updates",
            {
                {"callback", "function", "Function called on signature changes", "", 0, 0, false}
            },
            "listener_id (int)"
        },
        {
            "set_time",
            "Inject time for deterministic operation (Time Injection pattern)",
            {
                {"ticks", "int64", "Current time in milliseconds", "ms", 0, 0, false}
            },
            "void"
        }
    };

    // Inputs
    schema.inputs = {
        {
            "network_packets",
            "UDP packets from DJ Link devices on ports 50000, 50001, 50002",
            "binary",
            ""
        }
    };

    // Outputs
    schema.outputs = {
        {
            "device_announcement",
            "Information about discovered DJ devices",
            "json",
            ""
        },
        {
            "beat_event",
            "Beat timing information from a device",
            "json",
            ""
        },
        {
            "cdj_status",
            "Full CDJ/player status update",
            "json",
            ""
        }
    };

    return schema;
}

/**
 * JSON-formatted string for AI agent consumption.
 */
[[nodiscard]] inline std::string describe_api_json() {
    return describe_api().toJson();
}

} // namespace beatlink
