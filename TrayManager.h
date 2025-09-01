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
#include <functional>

struct TrayIcon {
    NOTIFYICONDATA nid{};
    HWND           targetWindow{ nullptr };
    std::wstring   windowTitle{};
    HICON          originalIcon{ nullptr };
    bool           wasMaximized{ false };
    bool           ownsIcon{ false };
    bool           isUwp{ false };
    int            originalDesktop{ 0 };
};

class TrayManager {
public:
    using ShowCollectionCallback = std::function<void()>;

    explicit TrayManager(HWND mainWindow, ShowCollectionCallback showCollectionCb);
    ~TrayManager();

    [[nodiscard]] bool AddWindowToTray(HWND hwnd, int originalDesktop, bool createIndividualIcon);
    [[nodiscard]] bool RestoreWindowFromTray(UINT iconId);
    void RestoreAllWindows();
    void RemoveAllTrayIcons();

    // The central handler for all tray icon messages
    [[nodiscard]] bool HandleTrayMessage(WPARAM wParam, LPARAM lParam);

    void SetCollectionMode(bool isEnabled);

    // Check if a window is already in the tray.
    [[nodiscard]] bool IsWindowInTray(HWND hwnd) const;

    // Get a constant reference to the tray icons map.
    const std::map<UINT, TrayIcon>& GetTrayIcons() const;

private:
    HWND                     mainWindow;
    std::map<UINT, TrayIcon> trayIcons;
    UINT                     nextIconId;

    bool                     collectionModeActive_;
    ShowCollectionCallback   showCollectionCallback_;

    HICON        GetWindowIcon(HWND hwnd, bool& owns);
    std::wstring GetWindowTitle(HWND hwnd);
    void         ShowContextMenu(POINT pt);
};

#endif