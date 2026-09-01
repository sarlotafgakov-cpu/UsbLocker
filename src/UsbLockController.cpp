// UsbLockController.cpp
#include "UsbLockController.h"
#include <iostream>

UsbLockController::UsbLockController(std::unique_ptr<IDeviceMonitor> mon,
                                     std::unique_ptr<ISystemLocker> lock,
                                     const TargetDevice& t)
    : monitor(std::move(mon)), locker(std::move(lock)), target(t) {}

void UsbLockController::start() {
    // Проверяем начальное состояние и синхронизируем реальную блокировку с ним:
    // без флешки — блокируем сразу при старте, с флешкой — разблокируем.
    bool present = monitor->isDevicePresent(target.vendor, target.product, target.serial);
    if (present) {
        std::cout << "Target device is present at startup. Unlocking." << std::endl;
        locker->unlock();
        isUnlocked = true;
    } else {
        std::cout << "Target device is absent at startup. Locking." << std::endl;
        locker->lock();
        isUnlocked = false;
    }

    // Запускаем мониторинг с колбэками
    monitor->start(
        [this](const UsbDeviceInfo& info) { onDeviceConnected(info); },
        [this](const UsbDeviceInfo& info) { onDeviceDisconnected(info); }
    );
}

void UsbLockController::stop() {
    monitor->stop();
}

void UsbLockController::onDeviceConnected(const UsbDeviceInfo& info) {
    // Проверяем, что это наше целевое устройство (по VID/PID/серийнику)
    if (info.vendorId == target.vendor &&
        info.productId == target.product &&
        info.serialNumber == target.serial) {
        std::cout << "Target device inserted. Unlocking system." << std::endl;
        updateState(true);
    }
}

void UsbLockController::onDeviceDisconnected(const UsbDeviceInfo& info) {
    // При отключении любого USB-устройства стоит перепроверить, не отключилась ли наша флешка.
    // Так как событие может прийти для другого устройства, проверяем наличие целевого.
    bool stillPresent = monitor->isDevicePresent(target.vendor, target.product, target.serial);
    if (!stillPresent) {
        std::cout << "Target device removed. Locking system." << std::endl;
        updateState(false);
    }
}

void UsbLockController::updateState(bool devicePresent) {
    if (devicePresent && !isUnlocked) {
        locker->unlock();
        isUnlocked = true;
    } else if (!devicePresent && isUnlocked) {
        locker->lock();
        isUnlocked = false;
    }
}
