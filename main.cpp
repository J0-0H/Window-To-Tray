/********************************************************************
 *  Window-To-Tray Main Entry Point
 *******************************************************************/
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <vector>
#include <algorithm>
#include <dwmapi.h>
#include <gdiplus.h>
#include "GlobalHook.h"
#include "TrayManager.h"
#include "WindowManager.h"
#include "VirtualDesktopManager.h"
#include "Resource.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Strings.h"
#include "CollectionWindow.h" 

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif
#ifndef NIN_KEYSELECT
#define NIN_KEYSELECT (WM_USER + 1)
#endif
#ifndef NIN_POPUPOPEN
#define NIN_POPUPOPEN 0x0406
#endif
#ifndef NIN_POPUPCLOSE
#define NIN_POPUPCLOSE 0x0407
#endif
#ifndef WM_TRAY_CALLBACK
#define WM_TRAY_CALLBACK (WM_USER + 1)
#endif
#define WM_MINIMIZE_TO_TRAY (WM_APP + 1)

class GdiplusManager {
public:
    GdiplusManager() {
        GdiplusStartupInput si;
        GdiplusStartup(&token_, &si, nullptr);
    }
    ~GdiplusManager() {
        GdiplusShutdown(token_);
    }
private:
    ULONG_PTR token_;
};

static UINT GetAppDpiForWindow(HWND hwnd) {
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32) {
        using PFN_GetDpiForWindow = UINT(WINAPI*)(HWND);
        auto pGetDpiForWindow = reinterpret_cast<PFN_GetDpiForWindow>(GetProcAddress(user32, "GetDpiForWindow"));
        if (pGetDpiForWindow) {
            UINT dpi = pGetDpiForWindow(hwnd);
            if (dpi > 0) return dpi;
        }
    }
    HDC hdc = GetDC(hwnd);
    if (hdc) {
        int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd, hdc);
        return dpi;
    }
    return 96;
}

static void EnableDpiAwareness()
{
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        using FNC_SetDpiAwarenessContext = BOOL(WINAPI*)(HANDLE);
        auto SetProcessDpiAwarenessContext =
            reinterpret_cast<FNC_SetDpiAwarenessContext>(
                ::GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (SetProcessDpiAwarenessContext &&
            SetProcessDpiAwarenessContext(reinterpret_cast<HANDLE>(-4)))
            return;
    }
    HMODULE shcore = ::LoadLibraryW(L"shcore.dll");
    if (shcore)
    {
        using FNC_SetProcessDpiAwareness = HRESULT(WINAPI*)(int);
        auto SetProcessDpiAwareness =
            reinterpret_cast<FNC_SetProcessDpiAwareness>(::GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (SetProcessDpiAwareness && SUCCEEDED(SetProcessDpiAwareness(2)))
        {
            ::FreeLibrary(shcore);
            return;
        }
        ::FreeLibrary(shcore);
    }
    if (user32)
    {
        auto SetProcessDPIAware =
            reinterpret_cast<BOOL(WINAPI*)()>(::GetProcAddress(user32, "SetProcessDPIAware"));
        if (SetProcessDPIAware) SetProcessDPIAware();
    }
}

static bool IsWindowCloaked(HWND hwnd) {
    BOOL cloaked = FALSE;
    ::DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return cloaked != FALSE;
}

static bool IntersectsMonitorWorkArea(HWND hwnd) {
    RECT rc{};
    if (!::GetWindowRect(hwnd, &rc)) return false;
    HMONITOR mon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (!mon) return false;
    MONITORINFO mi{ sizeof(mi) };
    if (!::GetMonitorInfoW(mon, &mi)) return false;
    RECT inter{};
    return ::IntersectRect(&inter, &rc, &mi.rcWork) != 0;
}

class WindowToTrayApp {
public:
    WindowToTrayApp();
    ~WindowToTrayApp();

    [[nodiscard]] bool Initialize(HINSTANCE hInstance);
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

    void OnMouseHook(POINT pt, HWND targetWindow);
    void CreateMainWindow(HINSTANCE);
    void CreateTrayIcon();
    void RemoveTrayIcon();

    void ApplySettingsToRuntime();
    void RegisterHotkeys();
    void UnregisterHotkeys();
    void OnHotkey(WPARAM hotkeyId);
    void MinimizeTopWindow();
    void HideAllVisibleWindows();
    void DisableCollectionModeAndSave();

    // --- NEW: Owner-drawn menu helpers ---
    void OnMeasureMenuItem(HWND hwnd, LPMEASUREITEMSTRUCT lpmis);
    void OnDrawMenuItem(HWND hwnd, LPDRAWITEMSTRUCT lpdis);
    void DrawMenuItemBackground(Graphics& g, LPDRAWITEMSTRUCT lpdis, UINT dpi);
    void DrawMenuItemSeparator(Graphics& g, LPDRAWITEMSTRUCT lpdis, UINT dpi);
    void DrawMenuItemText(Graphics& g, LPDRAWITEMSTRUCT lpdis);


    static WindowToTrayApp* instance;
    HINSTANCE               hInstance;
    HWND                    mainWindow;
    GlobalHook* mouseHook;
    TrayManager* trayManager;
    VirtualDesktopManager* virtualDesktopManager;
    NOTIFYICONDATA          mainTrayIcon;
    bool                    mainTrayIconCreated;

    HFONT                   hMenuFont_;
    UINT                    currentDpi_;

    SettingsManager         settingsMgr;
    Settings                settings;
};

WindowToTrayApp* WindowToTrayApp::instance = nullptr;

WindowToTrayApp::WindowToTrayApp()
    : hInstance(nullptr),
    mainWindow(nullptr),
    mouseHook(nullptr),
    trayManager(nullptr),
    virtualDesktopManager(nullptr),
    mainTrayIconCreated(false),
    hMenuFont_(nullptr),
    currentDpi_(96)
{
    instance = this;
    ::ZeroMemory(&mainTrayIcon, sizeof(mainTrayIcon));
}

WindowToTrayApp::~WindowToTrayApp()
{
    if (trayManager)
    {
        trayManager->RestoreAllWindows();
    }

    if (virtualDesktopManager)
    {
        virtualDesktopManager->ForceRemoveHiddenDesktop();
    }

    UnregisterHotkeys();
    RemoveTrayIcon();

    WindowManager::Cleanup(); // --- NEW: Cleanup WindowManager resources ---

    delete trayManager;
    delete mouseHook;
    delete virtualDesktopManager;
    instance = nullptr;

    if (hMenuFont_) {
        ::DeleteObject(hMenuFont_);
    }

    ::CoUninitialize();
}

bool WindowToTrayApp::Initialize(HINSTANCE hInst)
{
    hInstance = hInst;

    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        ::MessageBox(nullptr, I18N::S("error_com_init"), I18N::S("already_running_title"), MB_OK | MB_ICONERROR);
        return false;
    }

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    ::InitCommonControlsEx(&icc);

    settingsMgr.Load();
    settings = settingsMgr.Get();
    I18N::SetLanguage(settings.language);

    if (!WindowManager::Initialize()) // --- NEW: Initialize WindowManager ---
    {
        // Handle error if necessary, e.g., log or show a message
    }
    WindowManager::SetUseVirtualDesktop(settings.useVirtualDesktop);

    virtualDesktopManager = new VirtualDesktopManager();
    virtualDesktopManager->Initialize();
    WindowManager::SetVirtualDesktopManager(virtualDesktopManager);

    WNDCLASSEX wc{ sizeof(wc) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WindowToTrayClass";
    wc.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!::RegisterClassEx(&wc)) return false;

    CreateMainWindow(hInstance);
    if (!mainWindow) return false;

    trayManager = new TrayManager(mainWindow, [this]() {
        CollectionWindow::Show(
            this->mainWindow,
            *this->trayManager,
            CollectionWindow::PositioningMode::NearTray,
            [this] { this->DisableCollectionModeAndSave(); }
        );
        });

    mouseHook = new GlobalHook();
    mouseHook->SetMouseCallback(
        [this](POINT pt, HWND hwnd) { OnMouseHook(pt, hwnd); });
    if (!mouseHook->Install())
    {
        ::MessageBox(nullptr, L"Failed to install global mouse hook!",
            I18N::S("already_running_title"), MB_OK | MB_ICONERROR);
    }

    CreateTrayIcon();
    ApplySettingsToRuntime();
    RegisterHotkeys();

    return true;
}

void WindowToTrayApp::CreateMainWindow(HINSTANCE hInst)
{
    mainWindow = ::CreateWindowEx(
        0, L"WindowToTrayClass", L"Window-To-Tray",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        nullptr, nullptr, hInst, this);

    if (mainWindow) ::ShowWindow(mainWindow, SW_HIDE);
}

void WindowToTrayApp::CreateTrayIcon()
{
    mainTrayIcon.cbSize = sizeof(mainTrayIcon);
    mainTrayIcon.hWnd = mainWindow;
    mainTrayIcon.uID = 1;
    mainTrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    mainTrayIcon.uCallbackMessage = WM_TRAY_CALLBACK;
    mainTrayIcon.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    ::wcscpy_s(mainTrayIcon.szTip, I18N::S("tray_tooltip"));

    mainTrayIconCreated = ::Shell_NotifyIconW(NIM_ADD, &mainTrayIcon);
    if (mainTrayIconCreated)
    {
        mainTrayIcon.uVersion = NOTIFYICON_VERSION_4;
        ::Shell_NotifyIconW(NIM_SETVERSION, &mainTrayIcon);
    }
}

void WindowToTrayApp::RemoveTrayIcon()
{
    if (mainTrayIconCreated)
    {
        ::Shell_NotifyIconW(NIM_DELETE, &mainTrayIcon);
        mainTrayIconCreated = false;
    }
}

void WindowToTrayApp::ApplySettingsToRuntime()
{
    I18N::SetLanguage(settings.language);
    WindowManager::SetUseVirtualDesktop(settings.useVirtualDesktop);

    if (trayManager) {
        trayManager->SetCollectionMode(settings.useCollectionMode);
    }

    if (mainTrayIconCreated) {
        wcscpy_s(mainTrayIcon.szTip, I18N::S("tray_tooltip"));
        mainTrayIcon.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE;
        ::Shell_NotifyIconW(NIM_MODIFY, &mainTrayIcon);
    }
}

void WindowToTrayApp::RegisterHotkeys()
{
    UnregisterHotkeys();
    bool ok1 = true, ok2 = true, ok3 = true;

    if (settings.hkMinTop.vk) {
        ok1 = !!::RegisterHotKey(mainWindow, ID_HOTKEY_MIN_TOP, settings.hkMinTop.modifiers, settings.hkMinTop.vk);
    }
    if (settings.hkHideAll.vk) {
        ok2 = !!::RegisterHotKey(mainWindow, ID_HOTKEY_HIDE_ALL, settings.hkHideAll.modifiers, settings.hkHideAll.vk);
    }
    if (settings.hkShowCollection.vk) {
        ok3 = !!::RegisterHotKey(mainWindow, ID_HOTKEY_SHOW_COLLECTION, settings.hkShowCollection.modifiers, settings.hkShowCollection.vk);
    }

    if (!ok1 || !ok2 || !ok3) {
        ::MessageBoxW(mainWindow, I18N::S("error_hotkey_reg_failed"), I18N::S("already_running_title"), MB_OK | MB_ICONWARNING);
    }
}

void WindowToTrayApp::UnregisterHotkeys()
{
    ::UnregisterHotKey(mainWindow, ID_HOTKEY_MIN_TOP);
    ::UnregisterHotKey(mainWindow, ID_HOTKEY_HIDE_ALL);
    ::UnregisterHotKey(mainWindow, ID_HOTKEY_SHOW_COLLECTION);
}

void WindowToTrayApp::OnHotkey(WPARAM hotkeyId)
{
    if (hotkeyId == ID_HOTKEY_MIN_TOP) {
        MinimizeTopWindow();
    }
    else if (hotkeyId == ID_HOTKEY_HIDE_ALL) {
        HideAllVisibleWindows();
    }
    else if (hotkeyId == ID_HOTKEY_SHOW_COLLECTION) {
        CollectionWindow::Show(mainWindow, *trayManager, CollectionWindow::PositioningMode::CenterScreen,
            [this] { this->DisableCollectionModeAndSave(); });
    }
}

void WindowToTrayApp::MinimizeTopWindow()
{
    HWND tgt = ::GetForegroundWindow();

    if (!WindowManager::IsValidTargetWindow(tgt)) {
        POINT pt; ::GetCursorPos(&pt);
        HWND h = ::WindowFromPoint(pt);
        if (h) tgt = ::GetAncestor(h, GA_ROOT);
    }
    if (!WindowManager::IsValidTargetWindow(tgt)) return;

    int currentDesktop = 0;
    if (virtualDesktopManager && virtualDesktopManager->IsAvailable())
        currentDesktop = virtualDesktopManager->GetCurrentDesktopNumber();

    if (WindowManager::MinimizeToTray(tgt))
        trayManager->AddWindowToTray(tgt, currentDesktop, !settings.useCollectionMode);
}

void WindowToTrayApp::HideAllVisibleWindows()
{
    int currentDesktop = -1;
    if (virtualDesktopManager && virtualDesktopManager->IsAvailable()) {
        currentDesktop = virtualDesktopManager->GetCurrentDesktopNumber();
    }

    struct Ctx {
        WindowToTrayApp* app;
        int currentDesktop;
        std::vector<HWND>* list;
    } ctx{ this, currentDesktop, new std::vector<HWND>() };

    ::EnumWindows([](HWND hwnd, LPARAM lp)->BOOL {
        auto& c = *reinterpret_cast<Ctx*>(lp);

        if (!::IsWindow(hwnd)) return TRUE;
        if (hwnd == c.app->mainWindow) return TRUE;
        if (!WindowManager::IsValidTargetWindow(hwnd)) return TRUE;
        if (c.app->trayManager->IsWindowInTray(hwnd)) return TRUE;
        if (::IsIconic(hwnd)) return TRUE;
        if (IsWindowCloaked(hwnd)) return TRUE;

        if (c.currentDesktop >= 0 && c.app->virtualDesktopManager && c.app->virtualDesktopManager->IsAvailable()) {
            int wndDesk = c.app->virtualDesktopManager->GetWindowDesktopNumber(hwnd);
            if (wndDesk >= 0 && wndDesk != c.currentDesktop) return TRUE;
        }

        if (!IntersectsMonitorWorkArea(hwnd)) return TRUE;

        c.list->push_back(hwnd);
        return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

    for (HWND h : *ctx.list) {
        if (!::IsWindow(h)) continue;
        if (!WindowManager::IsValidTargetWindow(h)) continue;
        if (WindowManager::MinimizeToTray(h)) {
            int originalDesktop = (currentDesktop >= 0) ? currentDesktop : 0;
            trayManager->AddWindowToTray(h, originalDesktop, !settings.useCollectionMode);
        }
    }

    delete ctx.list;
}

void WindowToTrayApp::DisableCollectionModeAndSave()
{
    if (settings.useCollectionMode) {
        settings.useCollectionMode = false;
        settingsMgr.Set(settings);
        settingsMgr.Save();
        trayManager->RestoreAllWindows();
        ApplySettingsToRuntime();
    }
}

void WindowToTrayApp::OnMouseHook(POINT /*pt*/, HWND targetWindow)
{
    if (!WindowManager::IsValidTargetWindow(targetWindow)) return;

    ::PostMessage(mainWindow, WM_MINIMIZE_TO_TRAY,
        reinterpret_cast<WPARAM>(targetWindow), 0);
}

int WindowToTrayApp::Run()
{
    MSG msg;
    while (::GetMessage(&msg, nullptr, 0, 0))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WindowToTrayApp::WindowProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowToTrayApp* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (WindowToTrayApp*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else {
        pThis = (WindowToTrayApp*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    return pThis ?
        pThis->HandleMessage(hwnd, msg, wParam, lParam) :
        ::DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- NEW: Refactored owner-drawn menu implementation ---

void WindowToTrayApp::OnMeasureMenuItem(HWND hwnd, LPMEASUREITEMSTRUCT lpmis)
{
    if (lpmis->CtlType != ODT_MENU) return;

    UINT dpi = GetAppDpiForWindow(hwnd);

    // Custom separator height
    if (lpmis->itemID == (UINT)-1) {
        lpmis->itemWidth = 0; // Width is controlled by the menu
        lpmis->itemHeight = MulDiv(8, dpi, 96); // Separators are thin
        return;
    }

    // Ensure menu font is created for the current DPI
    if (hMenuFont_ == nullptr || currentDpi_ != dpi) {
        if (hMenuFont_) ::DeleteObject(hMenuFont_);
        currentDpi_ = dpi;
        LOGFONTW lf = {};
        lf.lfHeight = -MulDiv(10, dpi, 72); // 10pt font size
        lf.lfWeight = FW_NORMAL;
        wcscpy_s(lf.lfFaceName, L"Segoe UI Variable"); // Win11 default
        hMenuFont_ = ::CreateFontIndirectW(&lf);
        if (!hMenuFont_) { // Fallback
            wcscpy_s(lf.lfFaceName, L"Segoe UI");
            hMenuFont_ = ::CreateFontIndirectW(&lf);
        }
    }

    const wchar_t* text = reinterpret_cast<const wchar_t*>(lpmis->itemData);
    if (text && hMenuFont_) {
        HDC hdc = ::GetDC(hwnd);
        HFONT oldFont = (HFONT)::SelectObject(hdc, hMenuFont_);
        SIZE sz;
        ::GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
        ::SelectObject(hdc, oldFont);
        ::ReleaseDC(hwnd, hdc);

        // Add padding for a modern look
        lpmis->itemWidth = sz.cx + ::MulDiv(50, dpi, 96);
        lpmis->itemHeight = sz.cy + ::MulDiv(24, dpi, 96);
    }
}

void WindowToTrayApp::DrawMenuItemBackground(Graphics& g, LPDRAWITEMSTRUCT lpdis, UINT dpi)
{
    Color bgColor = Color(249, 249, 249);
    Color highlightColor = Color(234, 234, 234);

    RectF itemRect(REAL(lpdis->rcItem.left), REAL(lpdis->rcItem.top), REAL(lpdis->rcItem.right - lpdis->rcItem.left), REAL(lpdis->rcItem.bottom - lpdis->rcItem.top));
    SolidBrush bgBrush(bgColor);
    g.FillRectangle(&bgBrush, itemRect);

    if (lpdis->itemState & ODS_SELECTED) {
        SolidBrush highlightBrush(highlightColor);
        RectF highlightRect = itemRect;
        highlightRect.Inflate(-REAL(MulDiv(3, dpi, 96)), -REAL(MulDiv(3, dpi, 96)));

        GraphicsPath path;
        REAL r = REAL(MulDiv(4, dpi, 96)); // Corner radius
        path.AddArc(highlightRect.X, highlightRect.Y, r * 2, r * 2, 180, 90);
        path.AddArc(highlightRect.GetRight() - (r * 2), highlightRect.Y, r * 2, r * 2, 270, 90);
        path.AddArc(highlightRect.GetRight() - (r * 2), highlightRect.GetBottom() - (r * 2), r * 2, r * 2, 0, 90);
        path.AddArc(highlightRect.X, highlightRect.GetBottom() - (r * 2), r * 2, r * 2, 90, 90);
        path.CloseFigure();
        g.FillPath(&highlightBrush, &path);
    }
}

void WindowToTrayApp::DrawMenuItemSeparator(Graphics& g, LPDRAWITEMSTRUCT lpdis, UINT dpi)
{
    Color separatorColor = Color(220, 220, 220);
    Pen pen(separatorColor, 1.0f);
    int margin = MulDiv(16, dpi, 96);
    int y = lpdis->rcItem.top + (lpdis->rcItem.bottom - lpdis->rcItem.top) / 2;
    g.DrawLine(&pen, margin, y, lpdis->rcItem.right - margin, y);
}

void WindowToTrayApp::DrawMenuItemText(Graphics& g, LPDRAWITEMSTRUCT lpdis)
{
    const wchar_t* text = reinterpret_cast<const wchar_t*>(lpdis->itemData);
    if (!text || !hMenuFont_) return;

    Color textColor = Color(20, 20, 20);
    Color disabledColor = Color(160, 160, 160);

    SolidBrush textBrush((lpdis->itemState & ODS_DISABLED) ? disabledColor : textColor);
    Font gdiFont(lpdis->hDC, hMenuFont_);
    StringFormat stringFormat;
    stringFormat.SetAlignment(StringAlignmentNear);
    stringFormat.SetLineAlignment(StringAlignmentCenter);

    RectF textRect(REAL(lpdis->rcItem.left), REAL(lpdis->rcItem.top), REAL(lpdis->rcItem.right - lpdis->rcItem.left), REAL(lpdis->rcItem.bottom - lpdis->rcItem.top));
    textRect.X += REAL(MulDiv(20, currentDpi_, 96));
    textRect.Width -= REAL(MulDiv(25, currentDpi_, 96));

    g.DrawString(text, -1, &gdiFont, textRect, &stringFormat, &textBrush);
}

void WindowToTrayApp::OnDrawMenuItem(HWND, LPDRAWITEMSTRUCT lpdis)
{
    if (lpdis->CtlType != ODT_MENU) return;

    UINT dpi = GetAppDpiForWindow(lpdis->hwndItem);
    const UINT separatorId = (UINT)-1;

    Graphics graphics(lpdis->hDC);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    DrawMenuItemBackground(graphics, lpdis, dpi);

    if (lpdis->itemID == separatorId) {
        DrawMenuItemSeparator(graphics, lpdis, dpi);
    }
    else {
        DrawMenuItemText(graphics, lpdis);
    }
}

LRESULT WindowToTrayApp::HandleMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_MINIMIZE_TO_TRAY:
    {
        HWND tgt = reinterpret_cast<HWND>(wParam);
        int currentDesktop = 0;
        if (virtualDesktopManager && virtualDesktopManager->IsAvailable())
            currentDesktop = virtualDesktopManager->GetCurrentDesktopNumber();
        if (WindowManager::MinimizeToTray(tgt))
            trayManager->AddWindowToTray(tgt, currentDesktop, !settings.useCollectionMode);
        return 0;
    }
    case WM_HOTKEY:
        OnHotkey(wParam);
        return 0;
    case WM_TRAY_CALLBACK:
    {
        if (trayManager) {
            trayManager->HandleTrayMessage(wParam, lParam);
        }
        return 0;
    }
    case WM_MEASUREITEM:
        OnMeasureMenuItem(hwnd, (LPMEASUREITEMSTRUCT)lParam);
        return TRUE;
    case WM_DRAWITEM:
        OnDrawMenuItem(hwnd, (LPDRAWITEMSTRUCT)lParam);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_SETTINGS:
        {
            Settings newSettings;
            if (SettingsDialog::ShowModal(hwnd, settings, newSettings)) {
                settings = newSettings;
                settingsMgr.Set(settings);
                settingsMgr.Save();
                ApplySettingsToRuntime();
                RegisterHotkeys();
            }
            return 0;
        }
        case IDM_EXIT:
            ::PostQuitMessage(0);
            return 0;
        case IDM_RESTORE_ALL:
            trayManager->RestoreAllWindows();
            return 0;
        case IDM_ABOUT:
            ::MessageBox(hwnd, I18N::S("about_text"),
                I18N::S("about_title"), MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    EnableDpiAwareness();
    GdiplusManager gdiplus;

    HANDLE mutex = ::CreateMutex(nullptr, TRUE, L"WindowToTrayAppMutex");
    if (::GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ::MessageBox(nullptr, I18N::S("already_running"),
            I18N::S("already_running_title"), MB_OK | MB_ICONWARNING);
        if (mutex) ::CloseHandle(mutex);
        return 1;
    }

    WindowToTrayApp app;
    if (!app.Initialize(hInstance))
    {
        if (mutex) ::CloseHandle(mutex);
        return 1;
    }

    int ret = app.Run();
    if (mutex) ::CloseHandle(mutex);
    return ret;
}