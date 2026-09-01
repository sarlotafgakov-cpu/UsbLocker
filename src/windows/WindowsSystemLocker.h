// WindowsSystemLocker.h
#pragma once
#include "ISystemLocker.h"

class WindowsSystemLocker : public ISystemLocker {
public:
    void lock() override;
    void unlock() override; // см. .cpp: настоящая программная разблокировка невозможна

private:
    // Проверяет через WTS, заблокирована ли текущая сессия уже сейчас.
    // Нужно, чтобы lock() не дёргал LockWorkStation() повторно, пока
    // флешка отсутствует (сторож опрашивает раз в watchdogIntervalMs) —
    // это не мешает работе, но незачем спамить вызовами системного API.
    static bool isSessionLocked();
};
