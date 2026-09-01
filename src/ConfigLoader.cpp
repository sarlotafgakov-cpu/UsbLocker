// ConfigLoader.cpp
#include "ConfigLoader.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool fileExists(const std::string& path) {
    if (path.empty()) return false;
    std::ifstream f(path);
    return f.good();
}

bool parseBool(const std::string& value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return v == "true" || v == "1" || v == "yes" || v == "on";
}

} // namespace

std::string ConfigLoader::findConfigPath(const std::string& explicitPath) {
    // 1) путь, заданный явно (например, аргумент командной строки)
    if (!explicitPath.empty()) {
        if (fileExists(explicitPath)) return explicitPath;
        // Явно указанный, но не найденный путь — это ошибка конфигурации,
        // о ней сообщит loadTargetDevice().
        return "";
    }

    // 2) переменная окружения
    if (const char* env = std::getenv("USBLOCK_CONFIG")) {
        if (fileExists(env)) return env;
    }

    // 3) системное расположение (для установки как сервис)
    const std::string systemPath = "/etc/usblock/usblock.conf";
    if (fileExists(systemPath)) return systemPath;

    // 4) локальный конфиг рядом с проектом (удобно при разработке)
    const std::string localPath = "config/usblock.conf";
    if (fileExists(localPath)) return localPath;

    return "";
}

TargetDevice ConfigLoader::loadTargetDevice(const std::string& explicitPath) {
    std::string path = findConfigPath(explicitPath);
    if (path.empty()) {
        throw ConfigError(
            "Конфигурационный файл не найден. Укажите путь аргументом командной "
            "строки, переменной окружения USBLOCK_CONFIG, либо создайте "
            "/etc/usblock/usblock.conf или config/usblock.conf на основе "
            "config/usblock.conf.example.");
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        throw ConfigError("Не удалось открыть конфигурационный файл: " + path);
    }

    TargetDevice target;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            throw ConfigError(path + ":" + std::to_string(lineNo) +
                               ": ожидается строка вида key=value");
        }

        std::string key = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));
        std::transform(key.begin(), key.end(), key.begin(),
                        [](unsigned char c) { return std::tolower(c); });

        if (key == "vendor") target.vendor = value;
        else if (key == "product") target.product = value;
        else if (key == "serial") target.serial = value;
        else if (key == "debug") target.debug = parseBool(value);
        else if (key == "watchdog_interval_ms") {
            try {
                target.watchdogIntervalMs = std::stoi(value);
            } catch (const std::exception&) {
                throw ConfigError(path + ":" + std::to_string(lineNo) +
                                   ": watchdog_interval_ms должен быть целым числом");
            }
        }
        // неизвестные ключи молча игнорируются — расширяемость конфига
    }

    if (target.vendor.empty() || target.product.empty() || target.serial.empty()) {
        throw ConfigError(
            "В конфигурационном файле '" + path +
            "' должны быть заданы непустые vendor, product и serial.");
    }

    return target;
}
