// IDeviceMonitor.h
#pragma once
#include <functional>
#include <memory>
#include <string>

// Описание USB-устройства
struct UsbDeviceInfo {
    std::string vendorId;
    std::string productId;
    std::string serialNumber;
    std::string sysPath;   // уникальный идентификатор в системе
};

// Колбэки на события
using DeviceCallback = std::function<void(const UsbDeviceInfo&)>;

class IDeviceMonitor {
public:
    virtual ~IDeviceMonitor() = default;

    // Запустить мониторинг (передаём колбэки)
    virtual void start(DeviceCallback onConnected, DeviceCallback onDisconnected) = 0;
    virtual void stop() = 0;

    // Проверить, подключено ли целевое устройство прямо сейчас
    virtual bool isDevicePresent(const std::string& vendor,
                                 const std::string& product,
                                 const std::string& serial) = 0;
};
