#pragma once

namespace ztx {

enum class Theme { Dark, Light };
enum class Language { French, English };

struct Preferences {
    Theme theme = Theme::Dark;
    Language language = Language::French;
    bool animations = true;
    // 0: transparent and spacious; 100: opaque and compact.
    int glass = 50;
};

// Stores only the UI preferences in
// %LOCALAPPDATA%\CKitastam\ZTX\settings.ini. Missing/invalid values use
// defaults; glass is clamped to [0, 100]. These functions never throw.
// Saving replaces the complete INI atomically and reports any I/O failure.
[[nodiscard]] Preferences loadPreferences() noexcept;
[[nodiscard]] bool savePreferences(const Preferences& preferences) noexcept;

} // namespace ztx
