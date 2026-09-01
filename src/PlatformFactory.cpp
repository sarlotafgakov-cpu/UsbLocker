// PlatformFactory.cpp
#include "PlatformFactory.h"

#ifdef _WIN32
    #include "WindowsDeviceMonitor.h"
    #include "WindowsSystemLocker.h"
#elif __linux__
    #include "LinuxDeviceMonitor.h"
    #include "LinuxSystemLocker.h"
#else
    #error "Unsupported platform"
#endif

std::unique_ptr<IDeviceMonitor> createDeviceMonitor() {
#ifdef _WIN32
    return std::make_unique<WindowsDeviceMonitor>();
#elif __linux__
    return std::make_unique<LinuxDeviceMonitor>();
#endif
}

std::unique_ptr<ISystemLocker> createSystemLocker() {
#ifdef _WIN32
    return std::make_unique<WindowsSystemLocker>();
#elif __linux__
    return std::make_unique<LinuxSystemLocker>();
#endif
}
