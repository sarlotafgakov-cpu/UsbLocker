// WindowsDeviceMonitor.cpp
#include "WindowsDeviceMonitor.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <initguid.h>
#include <Usbiodef.h>   // для GUID_DEVINTERFACE_USB_DEVICE
#include <dbt.h>
#include <iostream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "setupapi.lib")

WindowsDeviceMonitor* WindowsDeviceMonitor::instance = nullptr;

namespace {

// Приводит hex-строку (VID/PID) к нижнему регистру, чтобы сравнение с
// конфигом (который обычно заполняют по образцу вывода lsusb/udev, т.е. в
// нижнем регистре) не зависело от регистра, в котором Windows отдаёт ID.
std::string toLowerHex(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string wideToNarrow(const wchar_t* wstr) {
    if (!wstr) return {};
    int len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, result.empty() ? nullptr : &result[0], len, nullptr, nullptr);
    return result;
}

// Разбирает строку вида "USB\VID_1234&PID_5678\SERIALNUMBER" (device
// instance id) или "\\?\USB#VID_1234&PID_5678#SERIALNUMBER#{guid}"
// (device interface path из DEV_BROADCAST_DEVICEINTERFACE). Оба формата
// используют одну и ту же схему полей, отличается только разделитель
// ('\' против '#'), поэтому ищем оба варианта одновременно.
bool parseUsbIdString(const std::string& raw, UsbDeviceInfo& info) {
    // Windows не различает регистр в путях устройств, но сама подстрока
    // "VID_"/"PID_" всегда в верхнем регистре — проверим как есть, а также
    // приведённую к верхнему регистру копию на случай нестандартного ввода.
    std::string upper = raw;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    size_t vidPos = upper.find("VID_");
    if (vidPos == std::string::npos || vidPos + 8 > raw.size()) return false;
    std::string vendor = raw.substr(vidPos + 4, 4);

    size_t pidPos = upper.find("PID_", vidPos);
    if (pidPos == std::string::npos || pidPos + 8 > raw.size()) return false;
    std::string product = raw.substr(pidPos + 4, 4);

    size_t afterPid = pidPos + 8;
    size_t sepPos = raw.find_first_of("#\\", afterPid);
    if (sepPos == std::string::npos) return false;

    size_t serialStart = sepPos + 1;
    size_t serialEnd = raw.find_first_of("#\\", serialStart);
    std::string serial = (serialEnd == std::string::npos)
        ? raw.substr(serialStart)
        : raw.substr(serialStart, serialEnd - serialStart);

    info.vendorId = toLowerHex(vendor);
    info.productId = toLowerHex(product);
    info.serialNumber = serial;
    info.sysPath = raw;
    return true;
}

} // namespace

WindowsDeviceMonitor::WindowsDeviceMonitor() {
    instance = this;
}

WindowsDeviceMonitor::~WindowsDeviceMonitor() { stop(); }

LRESULT CALLBACK WindowsDeviceMonitor::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DEVICECHANGE) {
        if (instance && (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)) {
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
            if (pHdr && pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                auto* pDevInterface = (PDEV_BROADCAST_DEVICEINTERFACE_A)pHdr;
                std::string path(pDevInterface->dbcc_name);

                UsbDeviceInfo info;
                bool parsed = parseUsbIdString(path, info);
                if (!parsed) {
                    // Не смогли разобрать VID/PID/серийник из пути — всё
                    // равно сообщаем о событии с пустыми полями, чтобы
                    // контроллер мог перепроверить текущее состояние через
                    // isDevicePresent() (так же ведёт себя Linux-версия при
                    // событиях, не относящихся к целевой флешке).
                    info.sysPath = path;
                }

                if (wParam == DBT_DEVICEARRIVAL) {
                    if (instance->onConnected) instance->onConnected(info);
                } else {
                    if (instance->onDisconnected) instance->onDisconnected(info);
                }
            }
        }
        return TRUE;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WindowsDeviceMonitor::run() {
    // Регистрируем класс окна
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "USBMonitorClass";
    RegisterClassA(&wc); 

    hwnd = CreateWindowExA(0, "USBMonitorClass", "", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        std::cerr << "Failed to create window" << std::endl;
        return;
    }

    // Подписываемся на уведомления об устройствах
    DEV_BROADCAST_DEVICEINTERFACE_A notificationFilter = {};
    notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_A);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    notificationFilter.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    HDEVNOTIFY hDevNotify = RegisterDeviceNotification(hwnd, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!hDevNotify) {
        std::cerr << "Failed to register device notification" << std::endl;
    }

    MSG msg;
    while (running && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        // Здесь можно обрабатывать события, но они уже идут через WndProc
    }

    if (hDevNotify) UnregisterDeviceNotification(hDevNotify);
    DestroyWindow(hwnd);
    hwnd = nullptr;
}

void WindowsDeviceMonitor::start(DeviceCallback onConn, DeviceCallback onDisc) {
    onConnected = std::move(onConn);
    onDisconnected = std::move(onDisc);
    running = true;
    worker = std::make_unique<std::thread>(&WindowsDeviceMonitor::run, this);
}

void WindowsDeviceMonitor::stop() {
    running = false;
    if (hwnd) PostMessage(hwnd, WM_QUIT, 0, 0);
    if (worker && worker->joinable()) worker->join();
}

bool WindowsDeviceMonitor::isDevicePresent(const std::string& vendor,
                                           const std::string& product,
                                           const std::string& serial) {
    // Перечисляем все присутствующие USB-устройства через SetupAPI и
    // сравниваем VID/PID/серийник каждого с целевыми. Это устраняет
    // прежнюю заглушку (всегда false), из-за которой сторож
    // (UsbPresenceWatchdog) считал, что флешки нет никогда, и постоянно
    // перезапускал блокировку экрана даже после того, как пользователь
    // вручную вводил пароль на экране блокировки Windows.
    std::string targetVendor = toLowerHex(vendor);
    std::string targetProduct = toLowerHex(product);

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(nullptr, "USB", nullptr,
                                              DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "SetupDiGetClassDevs failed: " << GetLastError() << std::endl;
        return false;
    }

    bool found = false;
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
        char instanceId[MAX_DEVICE_ID_LEN];
        if (!SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, instanceId,
                                          sizeof(instanceId), nullptr)) {
            continue;
        }

        UsbDeviceInfo info;
        if (!parseUsbIdString(instanceId, info)) continue;

        if (info.vendorId == targetVendor &&
            info.productId == targetProduct &&
            info.serialNumber == serial) {
            found = true;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return found;
}
