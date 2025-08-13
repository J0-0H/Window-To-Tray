#pragma once
#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <windows.h>
#include <shobjidl.h>

// Forward declaration
class VirtualDesktopManager;

class WindowManager {
public:
    // Set the virtual desktop manager instance
    static void SetVirtualDesktopManager(VirtualDesktopManager* vdm);

    // Set whether to use virtual desktop enhancement for UWP apps
    static void SetUseVirtualDesktop(bool enable);
    static bool GetUseVirtualDesktop();

    // Hides a window from the taskbar and Alt-Tab
    // UWP: Hides via traditional method, then moves to a hidden virtual desktop.
    // Win32: Uses traditional method only.
    static bool HideWindowFromTaskbar(HWND hwnd);

    // Restores a window to the taskbar and Alt-Tab
    // UWP: Moves back from the hidden virtual desktop, then restores traditionally.
    // Win32: Uses traditional method only.
    static bool ShowWindowInTaskbar(HWND hwnd, int originalDesktop = 0);

    // Checks if a window is a valid target for minimizing to tray
    static bool IsValidTargetWindow(HWND hwnd);

    // Unified entry point for "Minimize to Tray"
    static bool MinimizeToTray(HWND hwnd);

    // Checks if a window belongs to a UWP application
    static bool IsUWPApplication(HWND hwnd);

    // Tries to automatically remove the hidden desktop if it's no longer in use
    static void TryRemoveHiddenDesktopIfUnused();

private:
    static VirtualDesktopManager* virtualDesktopManager;
    static ITaskbarList3* taskbarList;
    static bool useVirtualDesktop;

    // Traditional hide/show methods (callable by both Win32 and UWP)
    static bool HideWindowTraditional(HWND hwnd);
    static bool ShowWindowTraditional(HWND hwnd);

    // Virtual desktop methods (enhancement for UWP apps)
    static bool HideWindowVirtual(HWND hwnd);
    static bool ShowWindowVirtual(HWND hwnd, int originalDesktop);

    // Helper functions for UWP detection
    static bool IsApplicationFrameHostWindow(HWND hwnd);
    static bool HasValidAUMID(HWND hwnd);

    // Helper to ensure the ITaskbarList3 instance is ready
    static bool EnsureTaskbar();
};

#endif // WINDOWMANAGER_H