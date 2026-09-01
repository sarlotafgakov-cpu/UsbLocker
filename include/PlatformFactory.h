// PlatformFactory.h
#pragma once
#include "IDeviceMonitor.h"
#include "ISystemLocker.h"
#include <memory>

std::unique_ptr<IDeviceMonitor> createDeviceMonitor();
std::unique_ptr<ISystemLocker> createSystemLocker();
