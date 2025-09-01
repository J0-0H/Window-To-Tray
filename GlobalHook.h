#pragma once
#ifndef GLOBALHOOK_H
#define GLOBALHOOK_H

#include <windows.h>
#include <functional>

class GlobalHook {
public:
    using MouseCallback = std::function<void(POINT, HWND)>;

    GlobalHook();
    ~GlobalHook();

    [[nodiscard]] bool Install();
    void Uninstall();

    void SetMouseCallback(MouseCallback cb);

private:
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static GlobalHook* s_instance;

    HHOOK mouseHook_ = nullptr;
    MouseCallback mouseCallback_;

    static HWND  s_lastHitHwnd;
    static POINT s_lastDownPt;
    static DWORD s_lastDownTick;

    [[nodiscard]] static bool IsMinimizeHit(HWND topLevel, POINT ptScreen);
    [[nodiscard]] static bool HitByTitleBarInfoEx(HWND hwnd, POINT ptScreen);
    [[nodiscard]] static bool HitByDwmCaptionButtonBounds(HWND hwnd, POINT ptScreen);
    [[nodiscard]] static bool HitByUIAutomation(HWND hwnd, POINT ptScreen);
    [[nodiscard]] static bool HitByNcHitTest(HWND hwnd, POINT ptScreen);

    static HWND GetTopLevelFromPoint(POINT ptScreen);

    static UINT  GetDpiForHwnd(HWND hwnd);
    static int   GetSystemMetricForDpi(int id, UINT dpi);
    static LRESULT SafeNcHitTest(HWND hwnd, POINT ptScreen);

    [[nodiscard]] static bool EnsureUIA();
};

#endif // GLOBALHOOK_H