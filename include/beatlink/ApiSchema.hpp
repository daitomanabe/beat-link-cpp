#pragma once

/**
 * API Introspection / Self-Description System
 * Per INTRODUCTION_JAVA_TO_CPP.md Section 2.1
 *
 * This module enables AI agents to discover the Beat Link API
 * without reading source code. When queried with {"cmd": "describe_api"}
 * or CLI --schema, it returns a JSON schema of all available operations.
 */

#include <string>
#include <vector>
#include <format>
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
        std::string json = std::format(
            R"({{"name":"{}","type":"{}","description":"{}")",
            name, type, description
        );
        if (!unit.empty()) {
            json += std::format(R"(,"unit":"{}")", unit);
        }
        if (has_range) {
            json += std::format(R"(,"min":{},"max":{})", min_value, max_value);
        }
        json += "}";
        return json;
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

        return std::format(
            R"({{"name":"{}","description":"{}","params":{},"returns":"{}"}})",
            name, description, params_json, returns
        );
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
        std::string json = std::format(
            R"({{"name":"{}","description":"{}","format":"{}")",
            name, description, format
        );
        if (!shape.empty()) {
            json += std::format(R"(,"shape":"{}")", shape);
        }
        json += "}";
        return json;
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

        return std::format(
            R"({{"name":"{}","version":"{}","description":"{}","commands":{},"inputs":{},"outputs":{}}})",
            name, version, description, commands_json, inputs_json, outputs_json
        );
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
    schema.description = "Pioneer DJ Link protocol library for C++20. "
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
            "get_devices",
            "Get list of currently discovered DJ devices",
            {},
            "array of DeviceInfo"
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
