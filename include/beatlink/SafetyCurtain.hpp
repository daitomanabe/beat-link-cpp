#pragma once

/**
 * Safety Curtain - Output Limiter Layer
 * Per INTRODUCTION_JAVA_TO_CPP.md Section 3.1
 *
 * This layer ensures that no matter what values are passed from
 * AI models or external inputs (including NaN/Inf), the output
 * is always within safe physical limits.
 *
 * For Beat Link, this primarily applies to:
 * - BPM values (sanity checking)
 * - Pitch/tempo percentages
 * - Device output signals
 */

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace beatlink {

/**
 * Safety limits for Beat Link values.
 */
struct SafetyLimits {
    // BPM limits (reasonable DJ range)
    static constexpr double MIN_BPM = 20.0;
    static constexpr double MAX_BPM = 300.0;

    // Pitch adjustment limits (typical CDJ range is +/- 16%, but some go to +/- 100%)
    static constexpr double MIN_PITCH_PERCENT = -100.0;
    static constexpr double MAX_PITCH_PERCENT = 100.0;

    // Beat within bar (always 1-4 for 4/4 time)
    static constexpr int MIN_BEAT = 1;
    static constexpr int MAX_BEAT = 4;

    // Device number (1-4 for standard, 9-12 for Opus Quad)
    static constexpr int MIN_DEVICE_NUMBER = 1;
    static constexpr int MAX_DEVICE_NUMBER = 12;
};

/**
 * Safety Curtain class - clamps all values to safe ranges.
 * All methods are constexpr and noexcept for real-time safety.
 */
class SafetyCurtain {
public:
    /**
     * Check if a floating-point value is safe (not NaN or Inf).
     */
    [[nodiscard]] static constexpr bool isSafe(double value) noexcept {
        return std::isfinite(value);
    }

    /**
     * Clamp a value to a range, handling NaN/Inf by returning a default.
     * This is the core safety function.
     */
    [[nodiscard]] static constexpr double clampSafe(
        double value,
        double min_val,
        double max_val,
        double default_val
    ) noexcept {
        if (!std::isfinite(value)) {
            return default_val;
        }
        return std::clamp(value, min_val, max_val);
    }

    /**
     * Sanitize a BPM value.
     * Returns a safe BPM value, defaulting to 120 if input is invalid.
     */
    [[nodiscard]] static constexpr double sanitizeBpm(double bpm) noexcept {
        constexpr double DEFAULT_BPM = 120.0;
        return clampSafe(bpm, SafetyLimits::MIN_BPM, SafetyLimits::MAX_BPM, DEFAULT_BPM);
    }

    /**
     * Sanitize a pitch percentage value.
     * Returns a safe pitch, defaulting to 0% (neutral) if input is invalid.
     */
    [[nodiscard]] static constexpr double sanitizePitchPercent(double pitch) noexcept {
        constexpr double DEFAULT_PITCH = 0.0;
        return clampSafe(pitch, SafetyLimits::MIN_PITCH_PERCENT, SafetyLimits::MAX_PITCH_PERCENT, DEFAULT_PITCH);
    }

    /**
     * Sanitize a beat position.
     * Returns a valid beat (1-4), defaulting to 1 if invalid.
     */
    [[nodiscard]] static constexpr int sanitizeBeat(int beat) noexcept {
        if (beat < SafetyLimits::MIN_BEAT || beat > SafetyLimits::MAX_BEAT) {
            return SafetyLimits::MIN_BEAT;
        }
        return beat;
    }

    /**
     * Sanitize a device number.
     * Returns a valid device number, defaulting to 1 if invalid.
     */
    [[nodiscard]] static constexpr int sanitizeDeviceNumber(int deviceNum) noexcept {
        if (deviceNum < SafetyLimits::MIN_DEVICE_NUMBER || deviceNum > SafetyLimits::MAX_DEVICE_NUMBER) {
            return SafetyLimits::MIN_DEVICE_NUMBER;
        }
        return deviceNum;
    }

    /**
     * Validate that raw packet data doesn't contain obviously bad values.
     * This is a quick sanity check before parsing.
     */
    [[nodiscard]] static constexpr bool validatePacketSize(size_t size, size_t expected_min) noexcept {
        // Protect against obviously malformed packets
        constexpr size_t MAX_REASONABLE_PACKET = 65536;
        return size >= expected_min && size <= MAX_REASONABLE_PACKET;
    }

    /**
     * Sanitize an integer value to a range.
     */
    [[nodiscard]] static constexpr int clampInt(int value, int min_val, int max_val) noexcept {
        return std::clamp(value, min_val, max_val);
    }

    /**
     * Sanitize a 64-bit integer value to a range.
     */
    [[nodiscard]] static constexpr int64_t clampInt64(int64_t value, int64_t min_val, int64_t max_val) noexcept {
        return std::clamp(value, min_val, max_val);
    }

private:
    SafetyCurtain() = delete; // Static utility class
};

/**
 * RAII guard that applies safety curtain on scope exit.
 * Usage:
 *   double bpm = untrusted_source;
 *   SafeValueGuard<double> guard(bpm, SafetyCurtain::sanitizeBpm);
 *   // bpm is now guaranteed safe
 */
template<typename T>
class SafeValueGuard {
public:
    template<typename Sanitizer>
    SafeValueGuard(T& value, Sanitizer sanitize) noexcept
        : value_(value) {
        value_ = sanitize(value_);
    }

private:
    T& value_;
};

} // namespace beatlink
