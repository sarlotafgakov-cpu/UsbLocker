CXX = g++
CXXFLAGS = -std=c++17 -Wall -Iinclude -Isrc/linux
LIBS = -ludev -lpthread

SRC = src/main.cpp \
      src/UsbLockController.cpp \
      src/UsbPresenceWatchdog.cpp \
      src/PlatformFactory.cpp \
      src/ConfigLoader.cpp \
      src/linux/LinuxDeviceMonitor.cpp \
      src/linux/LinuxSystemLocker.cpp

BIN = usblock

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN) $(LIBS)

clean:
	rm -f $(BIN)

.PHONY: all clean
