#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <cstring>
#include <span>
#include <concepts>
#include <optional>
#include <format>
#include <source_location>

namespace beatlink {

// ============================================================================
// C++20 Concepts (per INTRODUCTION_JAVA_TO_CPP.md)
// ============================================================================

/**
 * Concept: Type that can be used as a byte buffer
 */
template<typename T>
concept ByteBuffer = requires(T t) {
    { t.data() } -> std::convertible_to<const uint8_t*>;
    { t.size() } -> std::convertible_to<size_t>;
};

/**
 * Concept: Type that can receive beat updates
 */
template<typename T>
concept BeatReceiver = requires(T t, double bpm, int beatInBar) {
    { t.onBeat(bpm, beatInBar) } -> std::same_as<void>;
};

/**
 * Concept: Type that can be serialized to JSON
 */
template<typename T>
concept JsonSerializable = requires(T t) {
    { t.toJson() } -> std::convertible_to<std::string>;
};

// ============================================================================
// JSONL Structured Logging (per INTRODUCTION_JAVA_TO_CPP.md Section 2.3)
// ============================================================================

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

/**
 * JSONL formatted log entry for machine-readable logging.
 */
struct LogEntry {
    LogLevel level;
    std::string message;
    std::string source;
    int64_t timestamp_ms;

    [[nodiscard]] std::string toJsonl() const {
        const char* level_str = "info";
        switch (level) {
            case LogLevel::Debug:   level_str = "debug"; break;
            case LogLevel::Info:    level_str = "info"; break;
            case LogLevel::Warning: level_str = "warning"; break;
            case LogLevel::Error:   level_str = "error"; break;
        }
        return std::format(R"({{"level":"{}","message":"{}","source":"{}","timestamp_ms":{}}})",
                          level_str, message, source, timestamp_ms);
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Utility functions for the Beat Link protocol.
 * Ported from org.deepsymmetry.beatlink.Util
 *
 * Updated for C++20 per INTRODUCTION_JAVA_TO_CPP.md:
 * - std::span for buffer access
 * - std::format for string formatting
 * - std::optional for error handling
 * - Concepts for template constraints
 */
class Util {
public:
    /**
     * The sequence of ten bytes which begins all UDP packets sent in the protocol.
     */
    static constexpr std::array<uint8_t, 10> MAGIC_HEADER = {
        0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c
    };

    /**
     * The offset into protocol packets which identify the content of the packet.
     */
    static constexpr int PACKET_TYPE_OFFSET = 0x0a;

    /**
     * The value sent in beat and status packets to represent playback at normal speed.
     */
    static constexpr int64_t NEUTRAL_PITCH = 1048576;

    /**
     * Convert a signed byte to its unsigned equivalent in the range 0-255.
     */
    [[nodiscard]] static constexpr uint8_t unsign(int8_t b) noexcept {
        return static_cast<uint8_t>(b & 0xff);
    }

    /**
     * Reconstructs a number from bytes in big-endian order.
     * Uses std::span for safe buffer access (C++20).
     *
     * @param buffer the byte span containing the packet data
     * @param start the index of the first byte containing a numeric value
     * @param length the number of bytes making up the value
     * @return the reconstructed number, or std::nullopt on error
     */
    [[nodiscard]] static std::optional<int64_t> bytesToNumber(
        std::span<const uint8_t> buffer,
        size_t start,
        size_t length
    ) noexcept {
        if (start + length > buffer.size()) {
            return std::nullopt;
        }
        int64_t result = 0;
        for (size_t i = start; i < start + length; ++i) {
            result = (result << 8) + buffer[i];
        }
        return result;
    }

    // Legacy overload for raw pointers (deprecated, prefer std::span)
    [[nodiscard]] static int64_t bytesToNumber(const uint8_t* buffer, size_t start, size_t length) noexcept {
        int64_t result = 0;
        for (size_t i = start; i < start + length; ++i) {
            result = (result << 8) + buffer[i];
        }
        return result;
    }

    [[nodiscard]] static int64_t bytesToNumber(const std::vector<uint8_t>& buffer, size_t start, size_t length) noexcept {
        return bytesToNumber(buffer.data(), start, length);
    }

    /**
     * Reconstructs a number from bytes in little-endian order.
     */
    [[nodiscard]] static std::optional<int64_t> bytesToNumberLittleEndian(
        std::span<const uint8_t> buffer,
        size_t start,
        size_t length
    ) noexcept {
        if (start + length > buffer.size()) {
            return std::nullopt;
        }
        int64_t result = 0;
        for (size_t i = start + length; i > start; --i) {
            result = (result << 8) + buffer[i - 1];
        }
        return result;
    }

    // Legacy overload
    [[nodiscard]] static int64_t bytesToNumberLittleEndian(const uint8_t* buffer, size_t start, size_t length) noexcept {
        int64_t result = 0;
        for (size_t i = start + length; i > start; --i) {
            result = (result << 8) + buffer[i - 1];
        }
        return result;
    }

    /**
     * Writes a number to the specified byte span in big-endian order.
     */
    static void numberToBytes(int number, std::span<uint8_t> buffer, size_t start, size_t length) noexcept {
        if (start + length > buffer.size()) {
            return;
        }
        for (size_t i = start + length; i > start; --i) {
            buffer[i - 1] = static_cast<uint8_t>(number & 0xff);
            number >>= 8;
        }
    }

    // Legacy overload
    static void numberToBytes(int number, uint8_t* buffer, size_t start, size_t length) noexcept {
        for (size_t i = start + length; i > start; --i) {
            buffer[i - 1] = static_cast<uint8_t>(number & 0xff);
            number >>= 8;
        }
    }

    /**
     * Convert a pitch value to percentage (-100% to +100%).
     */
    [[nodiscard]] static constexpr double pitchToPercentage(int64_t pitch) noexcept {
        return (pitch - NEUTRAL_PITCH) / (NEUTRAL_PITCH / 100.0);
    }

    /**
     * Convert a pitch percentage to raw value.
     */
    [[nodiscard]] static constexpr int64_t percentageToPitch(double percentage) noexcept {
        return static_cast<int64_t>(percentage * NEUTRAL_PITCH / 100.0 + NEUTRAL_PITCH);
    }

    /**
     * Convert a pitch value to multiplier (0.0 to 2.0).
     */
    [[nodiscard]] static constexpr double pitchToMultiplier(int64_t pitch) noexcept {
        return pitch / static_cast<double>(NEUTRAL_PITCH);
    }

    /**
     * Validate that a packet starts with the magic header.
     * Uses std::span for safe buffer access.
     *
     * @param data the packet data as a span
     * @return true if the packet has a valid header
     */
    [[nodiscard]] static bool validateHeader(std::span<const uint8_t> data) noexcept {
        if (data.size() < MAGIC_HEADER.size()) {
            return false;
        }
        return std::memcmp(data.data(), MAGIC_HEADER.data(), MAGIC_HEADER.size()) == 0;
    }

    // Legacy overload
    [[nodiscard]] static bool validateHeader(const uint8_t* data, size_t length) noexcept {
        if (length < MAGIC_HEADER.size()) {
            return false;
        }
        return std::memcmp(data, MAGIC_HEADER.data(), MAGIC_HEADER.size()) == 0;
    }

    /**
     * Get the packet type byte from a packet.
     * Returns std::optional for safe error handling.
     */
    [[nodiscard]] static std::optional<uint8_t> getPacketType(std::span<const uint8_t> data) noexcept {
        if (data.size() <= static_cast<size_t>(PACKET_TYPE_OFFSET)) {
            return std::nullopt;
        }
        return data[PACKET_TYPE_OFFSET];
    }

    // Legacy overload
    [[nodiscard]] static uint8_t getPacketType(const uint8_t* data, size_t length) noexcept {
        if (length <= static_cast<size_t>(PACKET_TYPE_OFFSET)) {
            return 0;
        }
        return data[PACKET_TYPE_OFFSET];
    }

    /**
     * Convert half-frame number to time in milliseconds.
     * (75 frames per second, 150 half-frames)
     */
    [[nodiscard]] static constexpr int64_t halfFrameToTime(int64_t halfFrame) noexcept {
        return halfFrame * 100 / 15;
    }

    /**
     * Convert time in milliseconds to half-frame number.
     */
    [[nodiscard]] static constexpr int timeToHalfFrame(int64_t milliseconds) noexcept {
        return static_cast<int>(milliseconds * 15 / 100);
    }

    /**
     * Check if two addresses are on the same network.
     */
    [[nodiscard]] static constexpr bool sameNetwork(int prefixLength, uint32_t address1, uint32_t address2) noexcept {
        uint32_t prefixMask = 0xffffffff << (32 - prefixLength);
        return (address1 & prefixMask) == (address2 & prefixMask);
    }

    /**
     * Translate Opus Quad player numbers (9-12) to standard range (1-4).
     */
    [[nodiscard]] static constexpr int translateOpusPlayerNumbers(int reportedPlayerNumber) noexcept {
        return reportedPlayerNumber & 7;
    }

    /**
     * Extract a string from a byte span.
     */
    [[nodiscard]] static std::optional<std::string> extractString(
        std::span<const uint8_t> buffer,
        size_t offset,
        size_t length
    ) noexcept {
        if (offset + length > buffer.size()) {
            return std::nullopt;
        }
        std::string result(reinterpret_cast<const char*>(buffer.data() + offset), length);
        // Trim null characters and whitespace
        // Note: Can't use find_last_not_of with embedded null in literal, so trim manually
        while (!result.empty() && (result.back() == '\0' || result.back() == ' ' ||
               result.back() == '\t' || result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        return result;
    }

    // Legacy overload
    [[nodiscard]] static std::string extractString(const uint8_t* buffer, size_t offset, size_t length) noexcept {
        std::string result(reinterpret_cast<const char*>(buffer + offset), length);
        // Trim null characters and whitespace manually
        while (!result.empty() && (result.back() == '\0' || result.back() == ' ' ||
               result.back() == '\t' || result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        return result;
    }

    /**
     * Format a log message using std::format (C++20).
     */
    template<typename... Args>
    [[nodiscard]] static std::string formatLog(
        std::format_string<Args...> fmt,
        Args&&... args
    ) {
        return std::format(fmt, std::forward<Args>(args)...);
    }

    /**
     * Create a JSONL log entry with source location (C++20).
     */
    [[nodiscard]] static LogEntry createLogEntry(
        LogLevel level,
        const std::string& message,
        int64_t timestamp_ms,
        const std::source_location& loc = std::source_location::current()
    ) {
        return LogEntry{
            level,
            message,
            std::format("{}:{}", loc.file_name(), loc.line()),
            timestamp_ms
        };
    }

private:
    Util() = delete; // Prevent instantiation
};

} // namespace beatlink
