#pragma once
#ifndef GLOBALHOOK_H
#define GLOBALHOOK_H

#include <windows.h>
#include <functional>

/*
 * GlobalHook
 * - Uses WH_MOUSE_LL global low-level mouse hook to intercept right-clicks.
 * - When a right-click on a window's minimize button is detected, it suppresses
 *   the original mouse message and reports it via a callback.
 *
 * Callback Signature:
 *   void Callback(POINT ptScreen, HWND topLevelHwnd);
 *
 * Notes:
 * - Links against: user32.lib, dwmapi.lib, Ole32.lib, Uiautomationcore.lib
 * - Threading Model: The WH_MOUSE_LL callback executes on a system thread.
 *   UIA is initialized in MTA for compatibility.
 */
class GlobalHook {
public:
    using MouseCallback = std::function<void(POINT, HWND)>;

    GlobalHook();
    ~GlobalHook();

    bool Install();
    void Uninstall();

    void SetMouseCallback(MouseCallback cb);

private:
    // Low-level mouse hook procedure
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    // Singleton instance required for the static hook callback
    static GlobalHook* s_instance;

    // Hook handle
    HHOOK mouseHook_ = nullptr;

    // Callback function
    MouseCallback mouseCallback_;

    // State for the last mouse down event
    static HWND  s_lastHitHwnd;
    static POINT s_lastDownPt;
    static DWORD s_lastDownTick;

    // Main hit-testing entry point
    static bool IsMinimizeHit(HWND topLevel, POINT ptScreen);

    // Hit-testing methods (ordered from fastest to most compatible)
    static bool HitByTitleBarInfoEx(HWND hwnd, POINT ptScreen);
    static bool HitByDwmCaptionButtonBounds(HWND hwnd, POINT ptScreen);
    static bool HitByUIAutomation(HWND hwnd, POINT ptScreen);
    static bool HitByNcHitTest(HWND hwnd, POINT ptScreen);

    // Gets the top-level window that actually receives non-client messages
    static HWND GetTopLevelFromPoint(POINT ptScreen);

    // Utility functions: DPI, safe messaging, system metrics
    static UINT  GetDpiForHwnd(HWND hwnd);
    static int   GetSystemMetricForDpi(int id, UINT dpi);
    static LRESULT SafeNcHitTest(HWND hwnd, POINT ptScreen);

    // UIA initialization
    static bool EnsureUIA();
};

#endif // GLOBALHOOK_H