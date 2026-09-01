// LinuxSystemLocker.h
#pragma once
#include "ISystemLocker.h"
#include <string>

class LinuxSystemLocker : public ISystemLocker {
public:
    LinuxSystemLocker();
    void lock() override;
    void unlock() override;

private:
    std::string getSessionId();
    std::string sessionId;
};
