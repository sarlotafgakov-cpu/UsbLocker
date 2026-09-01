#pragma once
#include "IDeviceMonitor.h"
#include <thread>
#include <atomic>
#include <libudev.h>

class LinuxDeviceMonitor : public IDeviceMonitor {
public:
    LinuxDeviceMonitor();
    ~LinuxDeviceMonitor();

    void start(DeviceCallback onConnected, DeviceCallback onDisconnected) override;
    void stop() override;
    bool isDevicePresent(const std::string& vendor,
                         const std::string& product,
                         const std::string& serial) override;

private:
    void run();
    UsbDeviceInfo udevDeviceToInfo(udev_device* dev);

    std::unique_ptr<std::thread> worker;
    std::atomic<bool> running{false};
    DeviceCallback onConnected;
    DeviceCallback onDisconnected;
};
