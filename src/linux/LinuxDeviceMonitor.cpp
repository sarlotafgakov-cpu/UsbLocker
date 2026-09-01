#include "LinuxDeviceMonitor.h"
#include <unistd.h>
#include <iostream>
#include <cstring>

LinuxDeviceMonitor::LinuxDeviceMonitor() = default;
LinuxDeviceMonitor::~LinuxDeviceMonitor() { stop(); }

void LinuxDeviceMonitor::start(DeviceCallback onConn, DeviceCallback onDisc) {
    onConnected = std::move(onConn);
    onDisconnected = std::move(onDisc);
    running = true;
    worker = std::make_unique<std::thread>(&LinuxDeviceMonitor::run, this);
}

void LinuxDeviceMonitor::stop() {
    running = false;
    if (worker && worker->joinable()) worker->join();
}

void LinuxDeviceMonitor::run() {
    udev* udev_ctx = udev_new();
    if (!udev_ctx) {
        std::cerr << "Failed to create udev context" << std::endl;
        return;
    }

    udev_monitor* mon = udev_monitor_new_from_netlink(udev_ctx, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
    udev_monitor_enable_receiving(mon);

    int fd = udev_monitor_get_fd(mon);
    fd_set fds;
    struct timeval tv = {0, 100000}; // 100ms timeout для проверки флага running

    while (running) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) break;
        if (ret == 0) continue; // таймаут

        udev_device* dev = udev_monitor_receive_device(mon);
        if (!dev) continue;

        const char* action = udev_device_get_action(dev);
        UsbDeviceInfo info = udevDeviceToInfo(dev);

        // Проверяем, что это USB-устройство (не интерфейс)
        const char* devtype = udev_device_get_devtype(dev);
        if (devtype && strcmp(devtype, "usb_device") == 0) {
            if (strcmp(action, "add") == 0 && onConnected) {
                onConnected(info);
            } else if (strcmp(action, "remove") == 0 && onDisconnected) {
                onDisconnected(info);
            }
        }
        udev_device_unref(dev);
    }

    udev_monitor_unref(mon);
    udev_unref(udev_ctx);
}

UsbDeviceInfo LinuxDeviceMonitor::udevDeviceToInfo(udev_device* dev) {
    UsbDeviceInfo info;
    const char* vendor = udev_device_get_property_value(dev, "ID_VENDOR_ID");
    const char* product = udev_device_get_property_value(dev, "ID_MODEL_ID");
    const char* serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
    info.vendorId = vendor ? vendor : "";
    info.productId = product ? product : "";
    info.serialNumber = serial ? serial : "";
    const char* syspath = udev_device_get_syspath(dev);
    info.sysPath = syspath ? syspath : "";
    return info;
}

bool LinuxDeviceMonitor::isDevicePresent(const std::string& vendor,
                                         const std::string& product,
                                         const std::string& serial) {
    udev* udev_ctx = udev_new();
    if (!udev_ctx) return false;

    udev_enumerate* enumerate = udev_enumerate_new(udev_ctx);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);
    udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);

    bool found = false;
    udev_list_entry* entry;
    udev_list_entry_foreach(entry, devices) {
        const char* path = udev_list_entry_get_name(entry);
        udev_device* dev = udev_device_new_from_syspath(udev_ctx, path);
        if (!dev) continue;

        const char* v = udev_device_get_property_value(dev, "ID_VENDOR_ID");
        const char* p = udev_device_get_property_value(dev, "ID_MODEL_ID");
        const char* s = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
        if (v && p && s && vendor == v && product == p && serial == s) {
            found = true;
            udev_device_unref(dev);
            break;
        }
        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev_ctx);
    return found;
}
