// TargetDevice.h
#pragma once
#include <string>

// Описание "разрешающей" флешки: её VID/PID/серийный номер.
// Заполняется из конфигурационного файла (см. ConfigLoader), а не в коде.
struct TargetDevice {
    std::string vendor;
    std::string product;
    std::string serial;

    // debug=true  — сторожевой процесс (UsbPresenceWatchdog) при отсутствии
    //               флешки только пишет сообщение в консоль, систему не блокирует.
    // debug=false — сторожевой процесс блокирует систему (поведение по умолчанию).
    bool debug = false;

    // Интервал опроса UsbPresenceWatchdog в миллисекундах (по умолчанию 2000).
    int watchdogIntervalMs = 2000;
};
