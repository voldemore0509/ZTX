# Run from an MSYS2 UCRT64 shell. Install the UCRT64 GCC toolchain first.
CXX := g++
WINDRES := windres
TARGET := build/ZTX.exe
CPPFLAGS := -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Os -flto -ffunction-sections -fdata-sections
LDFLAGS := -municode -mwindows -static -static-libgcc -static-libstdc++ -flto -Wl,--gc-sections -s
LDLIBS := -lgdiplus -ldwmapi -lcomctl32 -lole32 -lshell32 -luuid -lgdi32 -luser32 -lshlwapi
OBJECTS := build/main.o build/preferences.o build/resources.o

.PHONY: all release clean
all: release
release: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o "$@"

build/%.o: src/%.cpp src/preferences.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

build/resources.o: resources/ztx.rc resources/resource.h resources/ztx.manifest resources/ztx.ico resources/logo.png | build
	cd resources && $(WINDRES) -I . --codepage=65001 -O coff ztx.rc -o ../build/resources.o

build:
	mkdir -p build

clean:
	$(RM) $(TARGET) $(OBJECTS) build/main.d build/preferences.d

-include build/main.d build/preferences.d
