// UsbLockController.h
#pragma once
#include "IDeviceMonitor.h"
#include "ISystemLocker.h"
#include "TargetDevice.h"
#include <memory>

class UsbLockController {
public:
    UsbLockController(std::unique_ptr<IDeviceMonitor> monitor,
                      std::unique_ptr<ISystemLocker> locker,
                      const TargetDevice& target);

    void start();
    void stop();

private:
    void onDeviceConnected(const UsbDeviceInfo& info);
    void onDeviceDisconnected(const UsbDeviceInfo& info);
    void updateState(bool devicePresent);

    std::unique_ptr<IDeviceMonitor> monitor;
    std::unique_ptr<ISystemLocker> locker;
    TargetDevice target;
    bool isUnlocked = false; // true = система разблокирована
};
