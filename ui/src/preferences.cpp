#include "preferences.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <limits>

namespace ztx {
namespace {

constexpr DWORD kPathCapacity = 32768;
constexpr wchar_t kSection[] = L"Interface";

// Fixed buffers keep the persistence path independent of heap allocation.
struct SettingsPath {
    wchar_t text[kPathCapacity];
    DWORD length = 0;

    bool append(const wchar_t* suffix) noexcept {
        const auto count = std::wcslen(suffix);
        if (count >= kPathCapacity - length) {
            return false;
        }
        std::wmemcpy(text + length, suffix, count + 1);
        length += static_cast<DWORD>(count);
        return true;
    }
};

bool ensureDirectory(const wchar_t* path) noexcept {
    if (CreateDirectoryW(path, nullptr)) {
        return true;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool getSettingsPath(SettingsPath& path, bool create) noexcept {
    path.length = GetEnvironmentVariableW(L"LOCALAPPDATA", path.text, kPathCapacity);
    if (path.length == 0 || path.length >= kPathCapacity) {
        return false;
    }
    while (path.length != 0 &&
           (path.text[path.length - 1] == L'\\' || path.text[path.length - 1] == L'/')) {
        path.text[--path.length] = L'\0';
    }
    if (path.length == 0 || !path.append(L"\\CKitastam") ||
        (create && !ensureDirectory(path.text)) || !path.append(L"\\ZTX") ||
        (create && !ensureDirectory(path.text))) {
        return false;
    }
    return path.append(L"\\settings.ini");
}

int readInteger(const wchar_t* file, const wchar_t* key, int fallback) noexcept {
    wchar_t value[32];
    const DWORD length = GetPrivateProfileStringW(kSection, key, L"", value, 32, file);
    if (length == 0 || length >= 31) {
        return fallback;
    }
    wchar_t* end = nullptr;
    errno = 0;
    const long result = std::wcstol(value, &end, 10);
    if (errno == ERANGE || end == value || *end != L'\0' ||
        result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
        return fallback;
    }
    return static_cast<int>(result);
}

} // namespace

Preferences loadPreferences() noexcept {
    Preferences result;
    SettingsPath path;
    if (!getSettingsPath(path, false)) {
        return result;
    }

    wchar_t value[16];
    GetPrivateProfileStringW(kSection, L"theme", L"dark", value, 16, path.text);
    if (std::wcscmp(value, L"light") == 0) {
        result.theme = Theme::Light;
    }
    GetPrivateProfileStringW(kSection, L"language", L"fr", value, 16, path.text);
    if (std::wcscmp(value, L"en") == 0) {
        result.language = Language::English;
    }
    const int animations = readInteger(path.text, L"animations", 1);
    result.animations = animations != 0;
    result.glass = std::clamp(readInteger(path.text, L"glass", 50), 0, 100);
    return result;
}

bool savePreferences(const Preferences& preferences) noexcept {
    SettingsPath path;
    if (!getSettingsPath(path, true)) {
        return false;
    }

    SettingsPath temporary;
    temporary.length = path.length;
    std::wmemcpy(temporary.text, path.text, static_cast<std::size_t>(path.length) + 1);
    wchar_t suffix[48];
    if (std::swprintf(suffix, 48, L".%lu.tmp", GetCurrentProcessId()) < 0 ||
        !temporary.append(suffix)) {
        return false;
    }

    // A full section is written to a sibling file before replacement, so a
    // failed write cannot leave a half-updated configuration behind.
    wchar_t section[128];
    const int length = std::swprintf(
        section, 128, L"theme=%ls|language=%ls|animations=%d|glass=%d|",
        preferences.theme == Theme::Light ? L"light" : L"dark",
        preferences.language == Language::English ? L"en" : L"fr",
        preferences.animations ? 1 : 0, std::clamp(preferences.glass, 0, 100));
    if (length < 0) {
        return false;
    }
    for (int index = 0; index < length; ++index) {
        if (section[index] == L'|') {
            section[index] = L'\0';
        }
    }

    const bool written = WritePrivateProfileSectionW(kSection, section, temporary.text) != FALSE;
    if (written) {
        // A cache flush returns zero even on success; it is not a write result.
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary.text);
    }
    const bool replaced = written && MoveFileExW(
        temporary.text, path.text, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!replaced) {
        DeleteFileW(temporary.text);
    }
    return replaced;
}

} // namespace ztx
