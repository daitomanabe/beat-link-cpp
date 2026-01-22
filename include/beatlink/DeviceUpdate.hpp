#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include "Span.hpp"
#include <asio.hpp>

#include "Util.hpp"

namespace beatlink {

/**
 * Abstract base class for device status updates.
 * Ported from org.deepsymmetry.beatlink.DeviceUpdate
 */
class DeviceUpdate {
public:
    /**
     * The offset at which the device number can be found.
     */
    static constexpr int DEVICE_NUMBER_OFFSET = 0x21;

    /**
     * Special device name for Opus Quad.
     */
    static constexpr const char* OPUS_NAME = "OPUS-QUAD";

    virtual ~DeviceUpdate() = default;

    /**
     * Get the IP address of the device.
     */
    asio::ip::address_v4 getAddress() const { return address_; }

    /**
     * Get the timestamp when this update was received.
     */
    std::chrono::steady_clock::time_point getTimestamp() const { return timestamp_; }

    /**
     * Get the age of this update in nanoseconds.
     */
    int64_t getTimestampNanos() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            timestamp_.time_since_epoch()).count();
    }

    /**
     * Get the device name.
     */
    const std::string& getDeviceName() const { return deviceName_; }

    /**
     * Get the device/player number.
     */
    int getDeviceNumber() const { return deviceNumber_; }

    /**
     * Check if this is a pre-Nexus CDJ.
     */
    bool isPreNexusCdj() const { return preNexusCdj_; }

    /**
     * Check if this came from an Opus Quad.
     */
    bool isFromOpusQuad() const { return isFromOpusQuad_; }

    /**
     * Get the raw packet bytes.
     */
    const std::vector<uint8_t>& getPacketBytes() const { return packetBytes_; }

    /**
     * Get the device pitch. Range 0-2097152 (0 = stopped, 1048576 = normal, 2097152 = 2x).
     */
    virtual int getPitch() const = 0;

    /**
     * Get the BPM * 100 (e.g., 12050 = 120.50 BPM).
     */
    virtual int getBpm() const = 0;

    /**
     * Check if this device is the tempo master.
     */
    virtual bool isTempoMaster() const = 0;

    /**
     * Check if this device is synced to the master.
     */
    virtual bool isSynced() const = 0;

    /**
     * Get the device number that master is being yielded to, if any.
     */
    virtual std::optional<int> getDeviceMasterIsBeingYieldedTo() const = 0;

    /**
     * Get the effective tempo (BPM adjusted by pitch).
     */
    virtual double getEffectiveTempo() const = 0;

    /**
     * Get the beat position within the bar (1-4).
     */
    virtual int getBeatWithinBar() const = 0;

    /**
     * Check if beat-within-bar is meaningful (players) vs arbitrary (mixers).
     */
    virtual bool isBeatWithinBarMeaningful() const = 0;

    /**
     * Get a string representation.
     */
    virtual std::string toString() const {
        return "DeviceUpdate[deviceNumber:" + std::to_string(deviceNumber_) +
               ", deviceName:" + deviceName_ +
               ", address:" + address_.to_string() + "]";
    }

protected:
    /**
     * Validate packet size for factory functions.
     * @return true if valid, false otherwise
     */
    [[nodiscard]] static bool validatePacketSize(size_t actualLength, size_t expectedLength) noexcept {
        return actualLength >= expectedLength;
    }

    /**
     * Constructor for subclasses (throwing version for backward compatibility).
     * @deprecated Use the noexcept version with pre-validated data
     */
    DeviceUpdate(const uint8_t* data, size_t length, const asio::ip::address_v4& senderAddress,
                 const std::string& packetName, size_t expectedLength, int deviceNumberOffset = DEVICE_NUMBER_OFFSET)
        : address_(senderAddress)
        , timestamp_(std::chrono::steady_clock::now())
    {
        if (length != expectedLength) {
            throw std::invalid_argument(packetName + " packet must be " +
                                        std::to_string(expectedLength) + " bytes long, got " +
                                        std::to_string(length));
        }

        initFromData(data, length, deviceNumberOffset);
    }

    /**
     * Protected noexcept constructor for factory functions.
     * Caller must ensure data is valid and of sufficient size.
     */
    struct NoValidateTag {};
    DeviceUpdate(std::span<const uint8_t> data, const asio::ip::address_v4& senderAddress,
                 int deviceNumberOffset, NoValidateTag) noexcept
        : address_(senderAddress)
        , timestamp_(std::chrono::steady_clock::now())
    {
        initFromData(data.data(), data.size(), deviceNumberOffset);
    }

private:
    void initFromData(const uint8_t* data, size_t length, int deviceNumberOffset) noexcept {
        packetBytes_.assign(data, data + length);
        deviceName_ = Util::extractString(data, 0x0b, 20);
        isFromOpusQuad_ = (deviceName_ == OPUS_NAME);

        // Check for pre-Nexus CDJ
        preNexusCdj_ = (deviceName_.find("CDJ") == 0) &&
                       (deviceName_.rfind("900") == deviceName_.length() - 3 ||
                        deviceName_.rfind("2000") == deviceName_.length() - 4);

        // Get device number (translate if Opus Quad)
        if (isFromOpusQuad_) {
            deviceNumber_ = Util::translateOpusPlayerNumbers(data[deviceNumberOffset]);
        } else {
            deviceNumber_ = data[deviceNumberOffset];
        }
    }

protected:

    asio::ip::address_v4 address_;
    std::chrono::steady_clock::time_point timestamp_;
    std::string deviceName_;
    int deviceNumber_;
    std::vector<uint8_t> packetBytes_;
    bool preNexusCdj_;
    bool isFromOpusQuad_;
};

} // namespace beatlink
