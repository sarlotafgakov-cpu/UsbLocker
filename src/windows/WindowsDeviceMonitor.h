// WindowsDeviceMonitor.h
#pragma once
#include "IDeviceMonitor.h"
#include <windows.h>
#include <thread>
#include <atomic>

class WindowsDeviceMonitor : public IDeviceMonitor {
public:
    WindowsDeviceMonitor();
    ~WindowsDeviceMonitor();

    void start(DeviceCallback onConnected, DeviceCallback onDisconnected) override;
    void stop() override;
    bool isDevicePresent(const std::string& vendor,
                         const std::string& product,
                         const std::string& serial) override;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void run();

    DeviceCallback onConnected;
    DeviceCallback onDisconnected;
    std::unique_ptr<std::thread> worker;
    std::atomic<bool> running{false};
    HWND hwnd = nullptr;
    static WindowsDeviceMonitor* instance; // для доступа из статического WndProc
};
