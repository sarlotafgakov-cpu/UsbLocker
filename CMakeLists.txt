cmake_minimum_required(VERSION 3.10)
project(UsbLock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Общие заголовки
include_directories(${CMAKE_SOURCE_DIR}/include)

set(COMMON_SRC
    src/main.cpp
    src/UsbLockController.cpp
    src/UsbPresenceWatchdog.cpp
    src/PlatformFactory.cpp
    src/ConfigLoader.cpp
)

if(WIN32)
    include_directories(${CMAKE_SOURCE_DIR}/src/windows)
    set(PLATFORM_SRC
        src/windows/WindowsDeviceMonitor.cpp
        src/windows/WindowsSystemLocker.cpp
    )
    set(EXTRA_LIBS user32 setupapi wtsapi32)
elseif(UNIX AND NOT APPLE)
    include_directories(${CMAKE_SOURCE_DIR}/src/linux)
    set(PLATFORM_SRC
        src/linux/LinuxDeviceMonitor.cpp
        src/linux/LinuxSystemLocker.cpp
    )
    find_library(UDEV_LIB udev)
    if(NOT UDEV_LIB)
        message(FATAL_ERROR "libudev not found. Install the libudev-dev package.")
    endif()
    set(EXTRA_LIBS ${UDEV_LIB} pthread)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

add_executable(UsbLock ${COMMON_SRC} ${PLATFORM_SRC})
target_link_libraries(UsbLock ${EXTRA_LIBS})

# Установка (необязательно): бинарник в bin/, пример конфига в /etc/usblock
install(TARGETS UsbLock RUNTIME DESTINATION bin)
install(FILES config/usblock.conf.example DESTINATION /etc/usblock)
