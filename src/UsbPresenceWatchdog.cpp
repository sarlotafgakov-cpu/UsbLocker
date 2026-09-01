// UsbPresenceWatchdog.cpp
#include "UsbPresenceWatchdog.h"
#include <iostream>

UsbPresenceWatchdog::UsbPresenceWatchdog(IDeviceMonitor& mon,
                                          ISystemLocker& lock,
                                          TargetDevice t,
                                          std::chrono::milliseconds interval)
    : monitor(mon), locker(lock), target(std::move(t)), pollInterval(interval) {}

UsbPresenceWatchdog::~UsbPresenceWatchdog() {
    stop();
}

void UsbPresenceWatchdog::start() {
    if (running) return; // уже запущен
    running = true;
    worker = std::thread(&UsbPresenceWatchdog::run, this);
}

void UsbPresenceWatchdog::stop() {
    running = false;
    if (worker.joinable()) worker.join();
}

void UsbPresenceWatchdog::run() {
    // Спим короткими интервалами, чтобы быстро реагировать на stop(),
    // а не ждать полный pollInterval при завершении программы.
    const auto tick = std::chrono::milliseconds(100);

    while (running) {
        auto slept = std::chrono::milliseconds(0);
        while (running && slept < pollInterval) {
            std::this_thread::sleep_for(tick);
            slept += tick;
        }
        if (!running) break;

        bool present = monitor.isDevicePresent(target.vendor, target.product, target.serial);
        if (!present) {
            if (target.debug) {
                std::cout << "[UsbPresenceWatchdog] Флешка не вставлена (debug-режим, "
                             "блокировка не выполняется)." << std::endl;
            } else {
                std::cout << "[UsbPresenceWatchdog] Флешка не вставлена. Блокировка системы."
                          << std::endl;
                locker.lock();
            }
        }
    }
}
