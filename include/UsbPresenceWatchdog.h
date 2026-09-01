// UsbPresenceWatchdog.h
#pragma once
#include "IDeviceMonitor.h"
#include "ISystemLocker.h"
#include "TargetDevice.h"
#include <atomic>
#include <chrono>
#include <thread>

// Отдельный фоновый процесс (поток), который независимо от UsbLockController
// периодически опрашивает, вставлена ли целевая флешка.
//
// Поведение при отсутствии флешки определяется флагом target.debug:
//   - debug == true  -> только сообщение в консоль ("флешка не вставлена"),
//                        система не блокируется;
//   - debug == false -> вызывается ISystemLocker::lock(), система блокируется.
//
// Это дополнительная подстраховка поверх событийного мониторинга
// (LinuxDeviceMonitor слушает udev add/remove): если событие удаления по
// какой-то причине не пришло, сторож всё равно обнаружит отсутствие флешки
// на очередном опросе.
class UsbPresenceWatchdog {
public:
    UsbPresenceWatchdog(IDeviceMonitor& monitor,
                         ISystemLocker& locker,
                         TargetDevice target,
                         std::chrono::milliseconds pollInterval = std::chrono::seconds(2));
    ~UsbPresenceWatchdog();

    // Запускает фоновый поток опроса. Повторный вызов без stop() — no-op.
    void start();

    // Останавливает поток и дожидается его завершения.
    void stop();

private:
    void run();

    IDeviceMonitor& monitor;
    ISystemLocker& locker;
    TargetDevice target;
    std::chrono::milliseconds pollInterval;

    std::atomic<bool> running{false};
    std::thread worker;
};
