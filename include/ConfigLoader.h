// ConfigLoader.h
#pragma once
#include "TargetDevice.h"
#include <string>
#include <stdexcept>

// Ошибка загрузки/парсинга конфигурации.
class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& msg) : std::runtime_error(msg) {}
};

class ConfigLoader {
public:
    // Загружает параметры целевого устройства из конфигурационного файла.
    //
    // explicitPath — путь, явно заданный пользователем (например, аргумент
    // командной строки). Если пуст, поиск идёт по стандартным местам:
    //   1) переменная окружения USBLOCK_CONFIG
    //   2) /etc/usblock/usblock.conf      (Linux, системная установка)
    //   3) ./config/usblock.conf          (запуск из каталога проекта)
    //
    // Бросает ConfigError, если файл не найден или обязательные поля
    // (vendor/product/serial) не заданы — намеренно, чтобы программа не
    // стартовала с "пустыми"/случайными значениями и не заблокировала
    // пользователя без возможности разблокировать систему.
    static TargetDevice loadTargetDevice(const std::string& explicitPath = "");

    // Возвращает путь к найденному конфигу или пустую строку, если не найден.
    static std::string findConfigPath(const std::string& explicitPath = "");
};
