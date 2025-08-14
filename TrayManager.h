#pragma once
#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

// Avoid min/max macro conflicts from <windows.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <map>
#include <string>

struct TrayIcon {
    NOTIFYICONDATA nid{};
    HWND           targetWindow{ nullptr };
    std::wstring   windowTitle{};
    HICON          originalIcon{ nullptr };
    bool           wasMaximized{ false };
    bool           ownsIcon{ false };
    bool           isUwp{ false };     // 新增：是否为 UWP 窗口
    int            originalDesktop{ 0 };
};

class TrayManager {
public:
    explicit TrayManager(HWND mainWindow);
    ~TrayManager();

    bool AddWindowToTray(HWND hwnd, int originalDesktop);
    bool RestoreWindowFromTray(UINT iconId);
    void RestoreAllWindows();
    void RemoveAllTrayIcons();

    bool HandleTrayMessage(WPARAM wParam, LPARAM lParam);

    // Check if a window is already in the tray.
    bool IsWindowInTray(HWND hwnd) const;

private:
    HWND                     mainWindow;
    std::map<UINT, TrayIcon> trayIcons;
    UINT                     nextIconId;

    HICON        GetWindowIcon(HWND hwnd, bool& owns);
    std::wstring GetWindowTitle(HWND hwnd);
    void         ShowTrayMenu(HWND hwnd, POINT pt);
};

#endif