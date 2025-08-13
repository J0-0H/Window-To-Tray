#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <windows.h>
#include <string>
#include "Strings.h"

struct Hotkey {
    UINT modifiers; // MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN
    UINT vk;        // Virtual-Key code
};

struct Settings {
    bool                useVirtualDesktop = true;
    I18N::Language      language = I18N::Language::Chinese;
    Hotkey              hkMinTop{ MOD_CONTROL | MOD_ALT, 'M' }; // Ctrl+Alt+M
    Hotkey              hkHideAll{ MOD_CONTROL | MOD_ALT, 'X' }; // Ctrl+Alt+X
};

class SettingsManager {
public:
    SettingsManager();

    bool Load();
    bool Save() const;

    const Settings& Get() const { return current_; }
    void Set(const Settings& s) { current_ = s; }

    static std::wstring GetProgramDirectoryW();
    static std::wstring GetSettingsPathW();

private:
    Settings current_;

    static std::wstring ToW(UINT v);
    static UINT FromIniInt(const wchar_t* section, const wchar_t* key, UINT def, const std::wstring& path);
    static bool FromIniBool(const wchar_t* section, const wchar_t* key, bool def, const std::wstring& path);
    static void WriteIniInt(const wchar_t* section, const wchar_t* key, UINT val, const std::wstring& path);
    static void WriteIniBool(const wchar_t* section, const wchar_t* key, bool val, const std::wstring& path);
};

#endif