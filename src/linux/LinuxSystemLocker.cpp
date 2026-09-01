// LinuxSystemLocker.cpp
#include "LinuxSystemLocker.h"
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <unistd.h>

LinuxSystemLocker::LinuxSystemLocker() {
    sessionId = getSessionId();
    if (sessionId.empty()) {
        std::cerr << "Warning: could not determine XDG session id. "
                     "Unlock via loginctl may not work; running as a systemd "
                     "service or via sudo/doas can hide XDG_SESSION_ID."
                  << std::endl;
    }
}

std::string LinuxSystemLocker::getSessionId() {
    // Сначала проверим переменную окружения (обычный интерактивный случай)
    if (const char* env = std::getenv("XDG_SESSION_ID")) {
        return std::string(env);
    }

    // Фоллбэк: переменная недоступна, если процесс запущен как сервис,
    // либо через sudo/doas. Спросим loginctl напрямую.
    FILE* pipe = popen("loginctl list-sessions --no-legend 2>/dev/null", "r");
    if (!pipe) return "";

    std::string firstSession;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::istringstream iss(buf);
        std::string candidate;
        iss >> candidate;
        if (!candidate.empty()) {
            firstSession = candidate;
            break; // берём первую найденную сессию
        }
    }
    pclose(pipe);
    return firstSession;
}

void LinuxSystemLocker::lock() {
    int ret = system("loginctl lock-session");
    if (ret != 0) {
        // fallback: если не получилось, попробуем с sessionId
        if (!sessionId.empty()) {
            std::string cmd = "loginctl lock-session " + sessionId;
            system(cmd.c_str());
        }
    }
}

void LinuxSystemLocker::unlock() {
    if (sessionId.empty()) {
        std::cerr << "Cannot unlock: session ID not found" << std::endl;
        return;
    }
    std::string cmd = "loginctl unlock-session " + sessionId;
    system(cmd.c_str());
}
