#include "Settings.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <Knownfolders.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

SettingsManager::SettingsManager() {}

std::wstring SettingsManager::GetProgramDirectoryW() {
    wchar_t path[MAX_PATH] = { 0 };
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? full.substr(0, pos) : L".";
}

// This implementation always saves settings to settings.ini in the exe's directory.
std::wstring SettingsManager::GetSettingsPathW() {
    std::wstring dir = GetProgramDirectoryW();
    return dir + L"\\settings.ini";
}

std::wstring SettingsManager::ToW(UINT v) {
    wchar_t buf[32]; wsprintfW(buf, L"%u", v); return buf;
}
UINT SettingsManager::FromIniInt(const wchar_t* section, const wchar_t* key, UINT def, const std::wstring& path) {
    return (UINT)::GetPrivateProfileIntW(section, key, (int)def, path.c_str());
}
bool SettingsManager::FromIniBool(const wchar_t* section, const wchar_t* key, bool def, const std::wstring& path) {
    wchar_t buf[16] = { 0 };
    ::GetPrivateProfileStringW(section, key, def ? L"1" : L"0", buf, ARRAYSIZE(buf), path.c_str());
    return (buf[0] == L'1' || buf[0] == L'Y' || buf[0] == L'y' || _wcsicmp(buf, L"true") == 0);
}
void SettingsManager::WriteIniInt(const wchar_t* section, const wchar_t* key, UINT val, const std::wstring& path) {
    ::WritePrivateProfileStringW(section, key, ToW(val).c_str(), path.c_str());
}
void SettingsManager::WriteIniBool(const wchar_t* section, const wchar_t* key, bool val, const std::wstring& path) {
    ::WritePrivateProfileStringW(section, key, val ? L"1" : L"0", path.c_str());
}

bool SettingsManager::Load() {
    const auto path = GetSettingsPathW();

    Settings s; // Load with defaults first

    // [General]
    s.useVirtualDesktop = FromIniBool(L"General", L"UseVirtualDesktop", s.useVirtualDesktop, path);
    UINT langNum = FromIniInt(L"General", L"Language", (UINT)s.language, path);
    s.language = (langNum == 1) ? I18N::Language::English : I18N::Language::Chinese;
    s.useCollectionMode = FromIniBool(L"General", L"UseCollectionMode", s.useCollectionMode, path); // --- NEW ---

    // [Hotkeys]
    s.hkMinTop.modifiers = FromIniInt(L"Hotkeys", L"MinTop_Mod", s.hkMinTop.modifiers, path);
    s.hkMinTop.vk = FromIniInt(L"Hotkeys", L"MinTop_Vk", s.hkMinTop.vk, path);

    s.hkHideAll.modifiers = FromIniInt(L"Hotkeys", L"HideAll_Mod", s.hkHideAll.modifiers, path);
    s.hkHideAll.vk = FromIniInt(L"Hotkeys", L"HideAll_Vk", s.hkHideAll.vk, path);

    // --- NEW ---
    s.hkShowCollection.modifiers = FromIniInt(L"Hotkeys", L"ShowCollection_Mod", s.hkShowCollection.modifiers, path);
    s.hkShowCollection.vk = FromIniInt(L"Hotkeys", L"ShowCollection_Vk", s.hkShowCollection.vk, path);

    current_ = s;
    return true;
}

bool SettingsManager::Save() const {
    const auto path = GetSettingsPathW();
    const auto& s = current_;

    // [General]
    WriteIniBool(L"General", L"UseVirtualDesktop", s.useVirtualDesktop, path);
    WriteIniInt(L"General", L"Language", (UINT)s.language, path);
    WriteIniBool(L"General", L"UseCollectionMode", s.useCollectionMode, path); // --- NEW ---

    // [Hotkeys]
    WriteIniInt(L"Hotkeys", L"MinTop_Mod", s.hkMinTop.modifiers, path);
    WriteIniInt(L"Hotkeys", L"MinTop_Vk", s.hkMinTop.vk, path);

    WriteIniInt(L"Hotkeys", L"HideAll_Mod", s.hkHideAll.modifiers, path);
    WriteIniInt(L"Hotkeys", L"HideAll_Vk", s.hkHideAll.vk, path);

    // --- NEW ---
    WriteIniInt(L"Hotkeys", L"ShowCollection_Mod", s.hkShowCollection.modifiers, path);
    WriteIniInt(L"Hotkeys", L"ShowCollection_Vk", s.hkShowCollection.vk, path);

    return true;
}