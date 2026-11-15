#include "preferences.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

unsigned checks = 0;

void require(bool condition, const char* description) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(description);
    }
}

void expect(const ztx::Preferences& actual, const ztx::Preferences& expected) {
    require(actual.theme == expected.theme, "Theme did not match");
    require(actual.language == expected.language, "Language did not match");
    require(actual.animations == expected.animations, "Animation setting did not match");
    require(actual.glass == expected.glass, "Glass setting did not match");
}

class Fixture {
public:
    Fixture() {
        const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (count != 0) {
            previousAppData_.resize(count);
            const DWORD copied = GetEnvironmentVariableW(
                L"LOCALAPPDATA", previousAppData_.data(), count);
            require(copied != 0 && copied < count, "Cannot preserve LOCALAPPDATA");
            previousAppData_.resize(copied);
            hadAppData_ = true;
        }
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    ~Fixture() {
        SetEnvironmentVariableW(L"LOCALAPPDATA", hadAppData_ ? previousAppData_.c_str() : nullptr);
        if (root_.empty()) {
            return;
        }
        // All targets are individually known children of the directory created
        // below. Never recurse, follow directory links or enumerate user data.
        const DWORD attributes = GetFileAttributesW(root_.c_str());
        if (root_.compare(0, tempBase_.size(), tempBase_) != 0 ||
            root_.size() <= tempBase_.size() || attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return;
        }
        const auto config = file();
        SetFileAttributesW(config.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(config.c_str());
        const auto pending = config + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
        DeleteFileW(pending.c_str());
        DeleteFileW((root_ + L"\\not-a-directory").c_str());
        RemoveDirectoryW((appData_ + L"\\CKitastam\\ZTX").c_str());
        RemoveDirectoryW((appData_ + L"\\CKitastam").c_str());
        RemoveDirectoryW(appData_.c_str());
        if (!RemoveDirectoryW(root_.c_str())) {
            std::fputs("WARNING: temporary test directory could not be removed.\n", stderr);
        }
    }

    void initialize() {
        std::array<wchar_t, 32768> buffer{};
        const DWORD count = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
        require(count != 0 && count < buffer.size(), "Cannot locate the temporary directory");
        std::array<wchar_t, 32768> resolved{};
        const DWORD absolute = GetFullPathNameW(
            buffer.data(), static_cast<DWORD>(resolved.size()), resolved.data(), nullptr);
        require(absolute != 0 && absolute < resolved.size(), "Cannot resolve the temporary directory");
        tempBase_.assign(resolved.data(), absolute);
        if (tempBase_.back() != L'\\') {
            tempBase_ += L'\\';
        }

        for (unsigned attempt = 0; attempt < 20; ++attempt) {
            const auto candidate = tempBase_ + L"ztx-preferences-test-" +
                std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt);
            if (CreateDirectoryW(candidate.c_str(), nullptr)) {
                root_ = candidate;
                break;
            }
            require(GetLastError() == ERROR_ALREADY_EXISTS, "Cannot create the test directory");
        }
        require(!root_.empty(), "Cannot reserve a unique test directory");
        // Exercise both non-ASCII characters and spaces in the storage path.
        appData_ = root_ + L"\\Donn\u00e9es utilisateur \u65e5\u672c";
        require(CreateDirectoryW(appData_.c_str(), nullptr) != FALSE,
                "Cannot create the Unicode app-data directory");
        useAppData(appData_);
    }

    void useAppData(const std::wstring& path) const {
        require(SetEnvironmentVariableW(L"LOCALAPPDATA", path.c_str()) != FALSE,
                "Cannot isolate LOCALAPPDATA");
    }

    std::wstring file() const { return appData_ + L"\\CKitastam\\ZTX\\settings.ini"; }
    const std::wstring& appData() const { return appData_; }
    const std::wstring& root() const { return root_; }

    void write(const wchar_t* key, const wchar_t* value) const {
        require(WritePrivateProfileStringW(L"Interface", key, value, file().c_str()) != FALSE,
                "Cannot prepare a test INI value");
    }

private:
    std::wstring previousAppData_;
    std::wstring tempBase_;
    std::wstring root_;
    std::wstring appData_;
    bool hadAppData_ = false;
};

void defaults(const Fixture& fixture) {
    expect(ztx::loadPreferences(), {});
    require(GetFileAttributesW((fixture.appData() + L"\\CKitastam").c_str()) ==
                INVALID_FILE_ATTRIBUTES,
            "Loading missing preferences unexpectedly created directories");
    std::puts("PASS: missing configuration uses defaults without writing");
}

void roundTrips(const Fixture& fixture) {
    for (const auto theme : {ztx::Theme::Dark, ztx::Theme::Light}) {
        for (const auto language : {ztx::Language::French, ztx::Language::English}) {
            for (const bool animations : {false, true}) {
                for (const int glass : {0, 1, 50, 99, 100}) {
                    const ztx::Preferences wanted{theme, language, animations, glass};
                    require(ztx::savePreferences(wanted), "Cannot save a valid preference combination");
                    expect(ztx::loadPreferences(), wanted);
                }
            }
        }
    }
    require(GetFileAttributesW(fixture.file().c_str()) != INVALID_FILE_ATTRIBUTES,
            "The INI was not created in the isolated Unicode directory");
    std::puts("PASS: all preference combinations round-trip through a Unicode path");
}

void bounds(const Fixture& fixture) {
    for (const int glass : {std::numeric_limits<int>::min(), -1, 101, std::numeric_limits<int>::max()}) {
        ztx::Preferences wanted;
        wanted.glass = glass;
        require(ztx::savePreferences(wanted), "Cannot save an out-of-range glass value");
        wanted.glass = glass < 0 ? 0 : 100;
        expect(ztx::loadPreferences(), wanted);
    }
    fixture.write(L"glass", L"-100");
    require(ztx::loadPreferences().glass == 0, "Negative INI glass was not clamped");
    fixture.write(L"glass", L"1000");
    require(ztx::loadPreferences().glass == 100, "Large INI glass was not clamped");
    std::puts("PASS: lower and upper bounds apply to saved and loaded values");
}

void malformed(const Fixture& fixture) {
    fixture.write(L"theme", L"unsupported");
    fixture.write(L"language", L"unsupported");
    fixture.write(L"animations", L"invalid");
    for (const auto value : {L"", L"invalid", L"42invalid", L"99999999999999999999999",
                            L"-99999999999999999999999", L"999999999999999999999999999999999999999"}) {
        fixture.write(L"glass", value);
        expect(ztx::loadPreferences(), {});
    }
    for (const auto key : {L"theme", L"language", L"animations", L"glass"}) {
        fixture.write(key, nullptr);
    }
    expect(ztx::loadPreferences(), {});
    std::puts("PASS: malformed, missing and overflowing values fall back safely");
}

void failures(const Fixture& fixture) {
    const ztx::Preferences saved{ztx::Theme::Light, ztx::Language::English, false, 25};
    require(ztx::savePreferences(saved), "Cannot prepare the save-failure test");
    const auto filename = fixture.file();
    require(SetFileAttributesW(filename.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE,
            "Cannot make the test configuration read-only");
    const bool protectedResult = ztx::savePreferences({});
    require(SetFileAttributesW(filename.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE,
            "Cannot restore the test configuration attributes");
    require(!protectedResult, "Saving unexpectedly replaced a read-only configuration");
    expect(ztx::loadPreferences(), saved);

    const auto badDirectory = fixture.root() + L"\\not-a-directory";
    const HANDLE file = CreateFileW(badDirectory.c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "Cannot create the invalid-directory fixture");
    CloseHandle(file);
    fixture.useAppData(badDirectory);
    require(!ztx::savePreferences({}), "Saving under a file path unexpectedly succeeded");
    expect(ztx::loadPreferences(), {});

    require(SetEnvironmentVariableW(L"LOCALAPPDATA", nullptr) != FALSE,
            "Cannot clear the test environment variable");
    require(!ztx::savePreferences({}), "Saving without LOCALAPPDATA unexpectedly succeeded");
    expect(ztx::loadPreferences(), {});

    fixture.useAppData(fixture.appData() + L"\\");
    require(ztx::savePreferences(saved), "A trailing path separator prevented saving");
    expect(ztx::loadPreferences(), saved);
    std::puts("PASS: failed writes preserve existing settings; invalid paths fail cleanly");
}

} // namespace

int main() {
    try {
        Fixture fixture;
        fixture.initialize();
        defaults(fixture);
        roundTrips(fixture);
        bounds(fixture);
        malformed(fixture);
        failures(fixture);
        std::printf("All preference tests passed (%u checks).\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s (Windows error %lu)\n", error.what(), GetLastError());
        return 1;
    }
}
