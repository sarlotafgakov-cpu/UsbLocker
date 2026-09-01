#include "UsbLockController.h"
#include "UsbPresenceWatchdog.h"
#include "PlatformFactory.h"
#include "TargetDevice.h"
#include "ConfigLoader.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> running{true};

void signalHandler(int) {
    running = false;
}

int main(int argc, char** argv) {
    // Путь к конфигу можно передать первым аргументом; иначе ConfigLoader
    // сам поищет его в стандартных местах (см. ConfigLoader.h).
    std::string configPath = (argc > 1) ? argv[1] : "";

    TargetDevice target;
    try {
        target = ConfigLoader::loadTargetDevice(configPath);
    } catch (const ConfigError& e) {
        std::cerr << "Ошибка конфигурации: " << e.what() << std::endl;
        return 1;
    }

    // Создаём компоненты через фабрику
    auto monitor = createDeviceMonitor();
    auto locker = createSystemLocker();

    // Запоминаем ссылки на monitor/locker до передачи владения в контроллер,
    // чтобы фоновый сторож (UsbPresenceWatchdog) мог использовать те же
    // объекты для независимого опроса наличия флешки.
    IDeviceMonitor& monitorRef = *monitor;
    ISystemLocker& lockerRef = *locker;

    UsbLockController controller(std::move(monitor), std::move(locker), target);

    // Фоновый процесс: периодически опрашивает, вставлена ли флешка.
    // В debug-режиме (target.debug == true) только пишет сообщение в
    // консоль, иначе — блокирует систему при её отсутствии.
    UsbPresenceWatchdog watchdog(monitorRef, lockerRef, target,
                                 std::chrono::milliseconds(target.watchdogIntervalMs));

    // Обработка Ctrl+C
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "USB Lock Controller started (target " << target.vendor << ":"
              << target.product << "). Press Ctrl+C to exit." << std::endl;
    if (target.debug) {
        std::cout << "Debug mode enabled: watchdog will only log to console, "
                     "not lock the system." << std::endl;
    }
    controller.start();
    watchdog.start();

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    watchdog.stop();
    controller.stop();
    std::cout << "Exiting." << std::endl;
    return 0;
}
