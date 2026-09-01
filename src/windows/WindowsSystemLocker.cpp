// WindowsSystemLocker.cpp
#include "WindowsSystemLocker.h"
#include <windows.h>
#include <wtsapi32.h>
#include <iostream>

#pragma comment(lib, "wtsapi32.lib")

bool WindowsSystemLocker::isSessionLocked() {
    WTSINFOEXW* info = nullptr;
    DWORD bytesReturned = 0;

    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                                      WTSSessionInfoEx, reinterpret_cast<LPWSTR*>(&info),
                                      &bytesReturned)) {
        // Не смогли определить состояние — считаем, что сессия не
        // заблокирована, чтобы не блокировать lock() из-за ошибки API.
        return false;
    }

    bool locked = false;
    if (info && info->Level == 1) {
        locked = (info->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK);
    }

    WTSFreeMemory(info);
    return locked;
}

void WindowsSystemLocker::lock() {
    // Не дёргаем LockWorkStation() повторно, если сессия уже заблокирована:
    // сторож (UsbPresenceWatchdog) опрашивает наличие флешки каждые
    // watchdogIntervalMs и без этой проверки вызывал бы блокировку заново
    // на каждом тике, пока флешка отсутствует.
    if (isSessionLocked()) return;

    if (!LockWorkStation()) {
        std::cerr << "Failed to lock workstation" << std::endl;
    }
}

void WindowsSystemLocker::unlock() {
    // В Windows нельзя программно разблокировать сессию без ввода пароля —
    // системный экран блокировки не даёт снять блокировку из стороннего
    // процесса без сохранённых учётных данных, что небезопасно и не
    // реализуется здесь намеренно.
    //
    // Реальная разблокировка на Windows выполняется пользователем вручную:
    // он вставляет флешку и вводит пароль на системном экране блокировки
    // как обычно. Роль этого метода — не разблокировать, а зафиксировать
    // в контроллере (UsbLockController::isUnlocked), что устройство сейчас
    // присутствует, чтобы приложение затем НЕ вызывало lock() повторно,
    // пока флешка не будет извлечена. Раньше это не работало, потому что
    // isDevicePresent() был заглушкой и всегда возвращал false — сторож
    // считал, что флешки нет никогда, и перезапускал блокировку сразу
    // после того, как пользователь вручную разблокировал сессию паролем.
    std::cout << "[WindowsSystemLocker] Флешка обнаружена. Автоматическая "
                 "разблокировка на Windows невозможна — введите пароль вручную; "
                 "пока флешка вставлена, повторной блокировки не будет."
              << std::endl;
}
