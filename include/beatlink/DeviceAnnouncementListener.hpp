#pragma once

#include <functional>
#include <memory>

namespace beatlink {

class DeviceAnnouncement;

class DeviceAnnouncementListener {
public:
    virtual ~DeviceAnnouncementListener() = default;
    virtual void deviceFound(const DeviceAnnouncement& announcement) = 0;
    virtual void deviceLost(const DeviceAnnouncement& announcement) = 0;
};

using DeviceAnnouncementListenerPtr = std::shared_ptr<DeviceAnnouncementListener>;

using DeviceAnnouncementCallback = std::function<void(const DeviceAnnouncement&)>;

class DeviceAnnouncementAdapter : public DeviceAnnouncementListener {
public:
    void deviceFound(const DeviceAnnouncement& /*announcement*/) override {}
    void deviceLost(const DeviceAnnouncement& /*announcement*/) override {}
};

class DeviceAnnouncementCallbacks final : public DeviceAnnouncementListener {
public:
    DeviceAnnouncementCallbacks(DeviceAnnouncementCallback found, DeviceAnnouncementCallback lost)
        : found_(std::move(found))
        , lost_(std::move(lost))
    {
    }

    void deviceFound(const DeviceAnnouncement& announcement) override {
        if (found_) {
            found_(announcement);
        }
    }

    void deviceLost(const DeviceAnnouncement& announcement) override {
        if (lost_) {
            lost_(announcement);
        }
    }

private:
    DeviceAnnouncementCallback found_;
    DeviceAnnouncementCallback lost_;
};

} // namespace beatlink
