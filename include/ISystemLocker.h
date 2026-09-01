// ISystemLocker.h
#pragma once

class ISystemLocker {
public:
    virtual ~ISystemLocker() = default;

    virtual void lock() = 0;      // заблокировать сессию
    virtual void unlock() = 0;    // разблокировать сессию
};
