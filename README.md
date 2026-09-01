# UsbLock — блокировка/разблокировка сессии по USB-флешке

Демон, который следит за USB-устройствами: если целевая флешка (по
VID/PID/серийнику) подключена — сессия разблокируется, если отключена —
блокируется.

## Структура проекта

```
Sandbox/
├── CMakeLists.txt
├── Makefile                    # быстрая сборка на Linux без CMake
├── config/
│   └── usblock.conf.example    # шаблон конфига, скопируйте в usblock.conf
├── include/                    # общие заголовки (кросс-платформенные)
│   ├── ConfigLoader.h
│   ├── IDeviceMonitor.h
│   ├── ISystemLocker.h
│   ├── PlatformFactory.h
│   ├── TargetDevice.h
│   └── UsbLockController.h
└── src/
    ├── main.cpp
    ├── ConfigLoader.cpp
    ├── PlatformFactory.cpp
    ├── UsbLockController.cpp
    ├── linux/                  # реализация для Linux (udev, loginctl)
    │   ├── LinuxDeviceMonitor.{h,cpp}
    │   └── LinuxSystemLocker.{h,cpp}
    └── windows/                # реализация для Windows (SetupAPI, WTS)
        ├── WindowsDeviceMonitor.{h,cpp}
        └── WindowsSystemLocker.{h,cpp}
```

Раньше все `.cpp`/`.h` лежали одним списком в корне репозитория — не было
видно ни слоёв (интерфейсы / общая логика / платформенный код), ни того,
что относится к Linux, а что к Windows. Теперь:

- `include/` — контракты и общая логика, не зависящая от ОС.
- `src/` — общая реализация (`main`, `UsbLockController`, `PlatformFactory`,
  `ConfigLoader`).
- `src/linux/`, `src/windows/` — платформенные реализации, компилируются
  только под свою ОС (это уже было в `CMakeLists.txt`, просто теперь и
  физическое расположение файлов это отражает).
- `config/` — конфигурация вместо хардкода в коде.

Файлы `UsbWatcher.h` и `UsbDevice.h` из корня старого проекта удалены —
они нигде не подключались и не собирались (дублировали `TargetDevice.h`/
`IDeviceMonitor.h`, оставшись от более раннего варианта архитектуры).

## Конфигурация (данные флешки теперь не в коде)

Раньше VID/PID/серийник были захардкожены прямо в `main.cpp`:

```cpp
target.vendor = "1234";
target.product = "5678";
target.serial = "ABCDEF";
```

Это и есть причина того, что "с флешкой разблокируется" могло работать
только случайно/тестово — реальные данные нужно было руками вписывать в
исходники и пересобирать проект при каждой замене флешки.

Теперь данные читаются из конфигурационного файла (`ConfigLoader`),
формат — простой `key=value`, одинаковый на обеих платформах:

```ini
vendor=1234
product=5678
serial=ABCDEF
```

Порядок поиска конфига (`ConfigLoader::findConfigPath`):

1. путь, переданный первым аргументом командной строки: `./UsbLock /path/to/usblock.conf`
2. переменная окружения `USBLOCK_CONFIG`
3. `/etc/usblock/usblock.conf` — для запуска как системный сервис (Linux)
4. `config/usblock.conf` — при запуске из каталога проекта (разработка)

**Важно:** если конфиг не найден или в нём не заполнены `vendor`/`product`/
`serial`, программа теперь завершается с понятной ошибкой и кодом возврата
1, а не стартует с "заглушкой" `1234/5678/ABCDEF`, которая никогда не
совпадёт ни с одной реальной флешкой и фактически навсегда заблокирует
сессию без возможности её снять флешкой.

Перед первым запуском (на любой ОС):

```bash
cp config/usblock.conf.example config/usblock.conf
# отредактируйте vendor/product/serial под свою флешку
```

---

## Linux

### Как узнать vendor/product/serial

```bash
lsusb                                              # найти строку вида ID 1234:5678
udevadm info --query=property --name=/dev/sdX | grep ID_SERIAL_SHORT
```

### Сборка

CMake (рекомендуется):

```bash
cmake -S . -B build
cmake --build build
./build/UsbLock config/usblock.conf   # либо без аргумента, если конфиг в /etc/usblock
```

Make, без CMake:

```bash
make
./usblock config/usblock.conf
```

Зависимости: `libudev-dev`, `pthread` (обычно уже есть).

```bash
sudo apt install libudev-dev   # Debian/Ubuntu
```

### Логика блокировки

- При старте программа сразу проверяет, воткнута ли целевая флешка:
  если да — разблокирует сессию, если нет — блокирует. Раньше при старте
  без флешки состояние просто "оставалось как есть" (программа не была
  уверена, заблокирован экран или нет); теперь начальное состояние
  синхронизируется явно.
- Дальше `LinuxDeviceMonitor` слушает события udev (`add`/`remove` для
  `usb_device`) и вызывает `unlock()`/`lock()` через `loginctl`.
- `LinuxSystemLocker` определяет сессию через `XDG_SESSION_ID`, а если эта
  переменная недоступна (например, программа запущена как systemd-сервис
  или через `sudo`/`doas`, где `XDG_SESSION_ID` не пробрасывается) —
  подстраховывается запросом `loginctl list-sessions`.

---

## Windows

### Как узнать vendor/product/serial

Все три значения видны в одной строке в Диспетчере устройств:

```
Диспетчер устройств → выбрать флешку → Свойства → вкладка "Сведения" →
свойство "ИД оборудования" (Hardware Ids) или "ИД экземпляра устройства"
(Device instance path)
```

Строка выглядит так:

```
USB\VID_1234&PID_5678\ABCDEF
      ^^^^      ^^^^   ^^^^^^
      vendor    product serial
```

То же самое можно получить командой PowerShell:

```powershell
Get-PnpDevice -Class USB | Select-Object InstanceId
```

### Сборка

Только через CMake (`Makefile` в проекте — только для Linux):

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\UsbLock.exe config\usblock.conf
```

Линкуются системные библиотеки `user32`, `setupapi`, `wtsapi32` — это уже
прописано в `CMakeLists.txt`, ничего доустанавливать не нужно (это часть
Windows SDK, который идёт вместе с Visual Studio / Build Tools).

Для работы блокировки/разблокировки программу нужно запускать в
пользовательской сессии (не как Windows-сервис из Session 0) — иначе
`LockWorkStation()` и `WTSQuerySessionInformation` будут работать не с той
сессией, которую видит пользователь.

### Логика блокировки

- `WindowsDeviceMonitor::isDevicePresent` перечисляет подключённые
  USB-устройства через `SetupDiGetClassDevs`/`SetupDiEnumDeviceInfo` +
  `SetupDiGetDeviceInstanceId`, разбирает VID/PID/серийник из строки device
  instance id (`USB\VID_xxxx&PID_yyyy\SERIAL`) и сравнивает их с конфигом.
- `WndProc` слушает `WM_DEVICECHANGE` (`DBT_DEVICEARRIVAL` /
  `DBT_DEVICEREMOVECOMPLETE`) и достаёт VID/PID/серийник из
  `DEV_BROADCAST_DEVICEINTERFACE::dbcc_name` (тот же формат полей, только
  через `#` вместо `\`), так что `UsbLockController` реагирует на события
  подключения/отключения сразу, а не только через периодический опрос
  `UsbPresenceWatchdog`.
- **Автоматическая разблокировка без ввода пароля на Windows невозможна** —
  системный экран блокировки не отдаёт управление стороннему процессу без
  сохранения учётных данных, а это небезопасно и намеренно не реализовано.
  Разблокировка на Windows выполняется пользователем вручную: вставил
  флешку → ввёл пароль на обычном экране блокировки Windows.
- Пока целевая флешка физически подключена, `isDevicePresent` возвращает
  `true`, поэтому ни `UsbPresenceWatchdog`, ни `UsbLockController` больше не
  вызывают `lock()` — сессия остаётся разблокированной после ручного ввода
  пароля до тех пор, пока флешка не будет извлечена.
- `WindowsSystemLocker::lock()` дополнительно проверяет текущее состояние
  сессии через `WTSQuerySessionInformation` (`WTSSessionInfoEx`) и не
  дублирует вызов `LockWorkStation()`, если сессия уже заблокирована.

---

## Известные ограничения

- Если на машине несколько активных пользовательских сессий одновременно,
  Linux-фоллбэк через `loginctl list-sessions` берёт первую из списка — для
  однопользовательского десктопа этого достаточно, но не подходит для
  мультисессионных серверов.
