#pragma once
#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <windows.h>
#include <shobjidl.h>

// Forward declaration
class VirtualDesktopManager;

class WindowManager {
public:
    // 初始化/清理
    static bool Initialize();
    static void Cleanup();

    // Set the virtual desktop manager instance
    static void SetVirtualDesktopManager(VirtualDesktopManager* vdm);

    // Set whether to use virtual desktop enhancement for UWP apps
    static void SetUseVirtualDesktop(bool enable);
    [[nodiscard]] static bool GetUseVirtualDesktop();

    // Hides a window from the taskbar and Alt-Tab
    [[nodiscard]] static bool HideWindowFromTaskbar(HWND hwnd);

    // Restores a window to the taskbar and Alt-Tab
    [[nodiscard]] static bool ShowWindowInTaskbar(HWND hwnd, int originalDesktop = 0);

    // Checks if a window is a valid target for minimizing to tray
    [[nodiscard]] static bool IsValidTargetWindow(HWND hwnd);

    // Unified entry point for "Minimize to Tray"
    [[nodiscard]] static bool MinimizeToTray(HWND hwnd);

    // Checks if a window belongs to a UWP application
    [[nodiscard]] static bool IsUWPApplication(HWND hwnd);

    // Tries to automatically remove the hidden desktop if it's no longer in use
    static void TryRemoveHiddenDesktopIfUnused();

private:
    static VirtualDesktopManager* virtualDesktopManager;
    static ITaskbarList3* taskbarList;
    static bool useVirtualDesktop;

    // Traditional hide/show methods (callable by both Win32 and UWP)
    [[nodiscard]] static bool HideWindowTraditional(HWND hwnd);
    [[nodiscard]] static bool ShowWindowTraditional(HWND hwnd);

    // Virtual desktop methods (enhancement for UWP apps)
    [[nodiscard]] static bool HideWindowVirtual(HWND hwnd);
    [[nodiscard]] static bool ShowWindowVirtual(HWND hwnd, int originalDesktop);

    // Helper functions for UWP detection
    [[nodiscard]] static bool IsApplicationFrameHostWindow(HWND hwnd);
    [[nodiscard]] static bool HasValidAUMID(HWND hwnd);

    // Helper to ensure the ITaskbarList3 instance is ready
    [[nodiscard]] static bool EnsureTaskbar();
};

#endif // WINDOWMANAGER_H