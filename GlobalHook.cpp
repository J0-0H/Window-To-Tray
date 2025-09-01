#include "GlobalHook.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <UIAutomation.h>
#include <cstdio>
#include <cmath>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uiautomationcore.lib")

#ifndef WM_GETTITLEBARINFOEX
#define WM_GETTITLEBARINFOEX 0x033F
#endif

// Static members
GlobalHook* GlobalHook::s_instance = nullptr;
HWND        GlobalHook::s_lastHitHwnd = nullptr;
POINT       GlobalHook::s_lastDownPt = { 0, 0 };
DWORD       GlobalHook::s_lastDownTick = 0;

// UIA singleton (lazy-loaded)
static IUIAutomation* g_uia = nullptr;
static bool g_uiaTriedInit = false;


// --- Utility Functions ---

static inline bool IsRectValid(const RECT& rc) {
    return (rc.right > rc.left) && (rc.bottom > rc.top);
}

static inline bool PtInRectEx(const RECT& rc, POINT pt) {
    return (pt.x >= rc.left && pt.x < rc.right && pt.y >= rc.top && pt.y < rc.bottom);
}

UINT GlobalHook::GetDpiForHwnd(HWND hwnd) {
    UINT dpi = 96;
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto pGetDpiForWindow = reinterpret_cast<UINT(WINAPI*)(HWND)>(::GetProcAddress(user32, "GetDpiForWindow"));
        auto pGetDpiForSystem = reinterpret_cast<UINT(WINAPI*)(void)>(::GetProcAddress(user32, "GetDpiForSystem"));
        if (pGetDpiForWindow) dpi = pGetDpiForWindow(hwnd);
        else if (pGetDpiForSystem) dpi = pGetDpiForSystem();
    }
    return dpi;
}

int GlobalHook::GetSystemMetricForDpi(int id, UINT dpi) {
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto pGetSystemMetricsForDpi = reinterpret_cast<int (WINAPI*)(int, UINT)>(::GetProcAddress(user32, "GetSystemMetricsForDpi"));
        if (pGetSystemMetricsForDpi) return pGetSystemMetricsForDpi(id, dpi);
    }
    return ::MulDiv(::GetSystemMetrics(id), dpi, 96);
}

LRESULT GlobalHook::SafeNcHitTest(HWND hwnd, POINT ptScreen) {
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    auto pGetWindowDpiAwarenessContext =
        reinterpret_cast<HANDLE(WINAPI*)(HWND)>(::GetProcAddress(user32, "GetWindowDpiAwarenessContext"));
    auto pSetThreadDpiAwarenessContext =
        reinterpret_cast<HANDLE(WINAPI*)(HANDLE)>(::GetProcAddress(user32, "SetThreadDpiAwarenessContext"));

    HANDLE oldCtx = nullptr;
    if (pGetWindowDpiAwarenessContext && pSetThreadDpiAwarenessContext) {
        HANDLE winCtx = pGetWindowDpiAwarenessContext(hwnd);
        oldCtx = pSetThreadDpiAwarenessContext(winCtx);
    }

    DWORD_PTR res = 0;
    LPARAM lp = MAKELPARAM((short)ptScreen.x, (short)ptScreen.y);
    BOOL ok = ::SendMessageTimeoutW(hwnd, WM_NCHITTEST, 0, lp,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 50, &res);

    if (pSetThreadDpiAwarenessContext && oldCtx) {
        pSetThreadDpiAwarenessContext(oldCtx);
    }
    return ok ? static_cast<LRESULT>(res) : HTNOWHERE;
}


// --- UI Automation ---

bool GlobalHook::EnsureUIA() {
    if (g_uia) return true;
    if (g_uiaTriedInit) return false;

    g_uiaTriedInit = true;

    HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE) {
        return false;
    }

    HRESULT hr = ::CoCreateInstance(CLSID_CUIAutomation, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_uia));
    return SUCCEEDED(hr) && g_uia;
}


// --- Top-Level Window Selection ---

HWND GlobalHook::GetTopLevelFromPoint(POINT ptScreen) {
    if (HWND cap = ::GetCapture()) {
        if (HWND top = ::GetAncestor(cap, GA_ROOT)) return top;
    }
    HWND hwnd = ::WindowFromPoint(ptScreen);
    if (!hwnd) return nullptr;
    HWND top = ::GetAncestor(hwnd, GA_ROOT);
    return top ? top : hwnd;
}


// --- Hit-Testing Logic ---

// 1) WM_GETTITLEBARINFOEX: Most reliable and fastest for standard title bars.
bool GlobalHook::HitByTitleBarInfoEx(HWND hwnd, POINT ptScreen) {
    TITLEBARINFOEX tbix{};
    tbix.cbSize = sizeof(tbix);
    if (!::SendMessageTimeoutW(hwnd, WM_GETTITLEBARINFOEX, 0,
        reinterpret_cast<LPARAM>(&tbix),
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 60, nullptr)) {
        return false;
    }
    const RECT& rcMin = tbix.rgrect[2];
    return IsRectValid(rcMin) && PtInRectEx(rcMin, ptScreen);
}

// 2) DWMWA_CAPTION_BUTTON_BOUNDS: For DWM-rendered custom frames.
//    --- REFINED OPTIMIZATION ---
bool GlobalHook::HitByDwmCaptionButtonBounds(HWND hwnd, POINT ptScreen) {
    RECT rcButtons{};
    HRESULT hr = ::DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS,
        &rcButtons, sizeof(rcButtons));
    if (FAILED(hr) || !IsRectValid(rcButtons)) return false;

    LONG style = static_cast<LONG>(::GetWindowLongW(hwnd, GWL_STYLE));
    if ((style & WS_MINIMIZEBOX) == 0) return false;

    int numButtons = 1; // Close button
    if (style & WS_MAXIMIZEBOX) numButtons++;
    // Minimize button already confirmed

    const LONG totalWidth = rcButtons.right - rcButtons.left;
    if (numButtons == 0 || totalWidth <= 0) return false;
    const int avgSlotWidth = totalWidth / numButtons;

    // --- NEW LOGIC: Refine the hitbox size ---
    // Use the average width to find the button's "slot", then use system metrics
    // to get a tighter hitbox, which is then centered within the slot.
    const UINT dpi = GetDpiForHwnd(hwnd);
    const int standardButtonWidth = GetSystemMetricForDpi(SM_CXSIZE, dpi);

    // The actual button width is likely the smaller of the two values.
    const int buttonWidth = min(avgSlotWidth, standardButtonWidth);

    // Calculate the padding needed to center the tighter hitbox within the slot.
    const int sidePadding = (avgSlotWidth > buttonWidth) ? (avgSlotWidth - buttonWidth) / 2 : 0;

    RECT rcMinSlot{};
    LONG exstyle = static_cast<LONG>(::GetWindowLongW(hwnd, GWL_EXSTYLE));
    const bool rtl = (exstyle & WS_EX_LAYOUTRTL) != 0;

    if (!rtl) { // LTR: Minimize button is the leftmost in the DWM group
        rcMinSlot.left = rcButtons.left;
        rcMinSlot.right = rcButtons.left + avgSlotWidth;
    }
    else { // RTL: Minimize button is the rightmost in the DWM group
        rcMinSlot.right = rcButtons.right;
        rcMinSlot.left = rcButtons.right - avgSlotWidth;
    }
    rcMinSlot.top = rcButtons.top;
    rcMinSlot.bottom = rcButtons.bottom;

    // Apply padding to shrink the slot to the final, tighter hitbox
    RECT rcMinHitbox = rcMinSlot;
    rcMinHitbox.left += sidePadding;
    rcMinHitbox.right -= sidePadding;

    return PtInRectEx(rcMinHitbox, ptScreen);
}

// 3) UI Automation: More reliable for custom-drawn title bars (e.g., Office, Chromium).
bool GlobalHook::HitByUIAutomation(HWND, POINT ptScreen) {
    if (!EnsureUIA() || !g_uia) return false;

    IUIAutomationElement* el = nullptr;
    HRESULT hr = g_uia->ElementFromPoint(ptScreen, &el);
    if (FAILED(hr) || !el) return false;

    bool hit = false;
    CONTROLTYPEID typeId = 0;
    if (SUCCEEDED(el->get_CurrentControlType(&typeId)) && typeId == UIA_ButtonControlTypeId) {
        RECT rc{};
        el->get_CurrentBoundingRectangle(&rc);

        BSTR name = nullptr, aid = nullptr;
        el->get_CurrentName(&name);
        el->get_CurrentAutomationId(&aid);

        auto containsWord = [](const wchar_t* s, const wchar_t* w)->bool {
            if (!s || !w) return false;
            return wcsstr(s, w) != nullptr;
            };

        const wchar_t* minimizeKeywords[] = {
            L"Minimize", L"最小化", L"Minimieren", L"Minimiser", L"Minimizar",
            L"Riduci a icona", L"Свернуть", L"最小化", L"최소화", L"Minimalizuj"
        };
        bool nameMatch = false;
        if (name) {
            for (const auto& keyword : minimizeKeywords) {
                if (containsWord(name, keyword)) {
                    nameMatch = true;
                    break;
                }
            }
        }
        const bool idMatch = containsWord(aid, L"Min");

        if ((nameMatch || idMatch) && IsRectValid(rc) && PtInRect(&rc, ptScreen)) {
            hit = true;
        }

        if (name) ::SysFreeString(name);
        if (aid)  ::SysFreeString(aid);
    }

    el->Release();
    return hit;
}

// 4) WM_NCHITTEST: The final fallback.
bool GlobalHook::HitByNcHitTest(HWND hwnd, POINT ptScreen) {
    LRESULT ht = SafeNcHitTest(hwnd, ptScreen);
    return (ht == HTMINBUTTON);
}

// Main hit-testing entry point with short-circuiting.
bool GlobalHook::IsMinimizeHit(HWND topLevel, POINT ptScreen) {
    if (!topLevel || !::IsWindowVisible(topLevel)) return false;

    if (HitByTitleBarInfoEx(topLevel, ptScreen)) return true;
    if (HitByDwmCaptionButtonBounds(topLevel, ptScreen)) return true;
    if (HitByUIAutomation(topLevel, ptScreen)) return true;
    if (HitByNcHitTest(topLevel, ptScreen)) return true;

    return false;
}


// --- Lifecycle ---

GlobalHook::GlobalHook() {
    s_instance = this;
}

GlobalHook::~GlobalHook() {
    Uninstall();
    if (g_uia) {
        g_uia->Release();
        g_uia = nullptr;
    }
    s_instance = nullptr;
}

bool GlobalHook::Install() {
    if (mouseHook_) return true;
    mouseHook_ = ::SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc,
        ::GetModuleHandleW(nullptr), 0);
    return mouseHook_ != nullptr;
}

void GlobalHook::Uninstall() {
    if (mouseHook_) {
        ::UnhookWindowsHookEx(mouseHook_);
        mouseHook_ = nullptr;
    }
}

void GlobalHook::SetMouseCallback(MouseCallback cb) {
    mouseCallback_ = std::move(cb);
}


// --- Hook Procedure ---

LRESULT CALLBACK GlobalHook::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance) {
        const auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        const POINT pt = ms->pt;

        switch (wParam) {
        case WM_RBUTTONDOWN: {
            HWND top = GetTopLevelFromPoint(pt);
            if (IsMinimizeHit(top, pt)) {
                s_lastHitHwnd = top;
                s_lastDownPt = pt;
                s_lastDownTick = ::GetTickCount();
                return 1; // Suppress message
            }
            else {
                s_lastHitHwnd = nullptr;
            }
            break;
        }
        case WM_RBUTTONUP: {
            const DWORD kClickTimeoutMs = 800;
            const int   kMoveTolerance = 5;

            HWND top = GetTopLevelFromPoint(pt);
            if (s_lastHitHwnd && top == s_lastHitHwnd) {
                const bool withinTime = (::GetTickCount() - s_lastDownTick) <= kClickTimeoutMs;
                const bool withinMove = (std::abs(pt.x - s_lastDownPt.x) <= kMoveTolerance) &&
                    (std::abs(pt.y - s_lastDownPt.y) <= kMoveTolerance);
                if (withinTime && withinMove && IsMinimizeHit(top, pt)) {
                    if (s_instance->mouseCallback_) {
                        s_instance->mouseCallback_(pt, top);
                    }
                    s_lastHitHwnd = nullptr;
                    return 1; // Suppress message
                }
            }
            s_lastHitHwnd = nullptr;
            break;
        }
        default:
            break;
        }
    }
    return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}