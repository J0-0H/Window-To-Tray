/********************************************************************
 *  Window-To-Tray Main Entry Point
 *******************************************************************/
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <vector>
#include <algorithm>
#include <dwmapi.h>
#include "GlobalHook.h"
#include "TrayManager.h"
#include "WindowManager.h"
#include "VirtualDesktopManager.h"
#include "Resource.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Strings.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

 // Compatibility definitions for NIN_* constants
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

// Define WM_TRAY_CALLBACK
#ifndef WM_TRAY_CALLBACK
#define WM_TRAY_CALLBACK (WM_USER + 1)
#endif

// Custom message to perform "minimize to tray" on the UI thread
#define WM_MINIMIZE_TO_TRAY (WM_APP + 1)


// Enable Per-Monitor-DPI Awareness
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
            SetProcessDpiAwarenessContext(reinterpret_cast<HANDLE>(-4))) // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            return;
    }
    HMODULE shcore = ::LoadLibraryW(L"shcore.dll");
    if (shcore)
    {
        using FNC_SetProcessDpiAwareness = HRESULT(WINAPI*)(int);
        auto SetProcessDpiAwareness =
            reinterpret_cast<FNC_SetProcessDpiAwareness>(::GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (SetProcessDpiAwareness &&
            SUCCEEDED(SetProcessDpiAwareness(2))) // PROCESS_PER_MONITOR_DPI_AWARE
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

// Helper filters: only process windows actually visible on the current desktop
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

    bool Initialize(HINSTANCE hInstance);
    int  Run();

private:
    // Window & Messages
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

    // Internal Helpers
    void OnMouseHook(POINT pt, HWND targetWindow);
    void CreateMainWindow(HINSTANCE);
    void CreateTrayIcon();
    void RemoveTrayIcon();

    // Settings & Hotkeys
    void ApplySettingsToRuntime();
    void RegisterHotkeys();
    void UnregisterHotkeys();
    void OnHotkey(WPARAM hotkeyId);
    void MinimizeTopWindow();
    void HideAllVisibleWindows();

    // Data Members
    static WindowToTrayApp* instance;
    HINSTANCE               hInstance;
    HWND                    mainWindow;
    GlobalHook* mouseHook;
    TrayManager* trayManager;
    VirtualDesktopManager* virtualDesktopManager;
    NOTIFYICONDATA          mainTrayIcon;
    bool                    mainTrayIconCreated;

    // Settings
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
    mainTrayIconCreated(false)
{
    instance = this;
    ::ZeroMemory(&mainTrayIcon, sizeof(mainTrayIcon));
}

WindowToTrayApp::~WindowToTrayApp()
{
    // Restore all windows and remove the hidden desktop before exiting
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
    delete trayManager;
    delete mouseHook;
    delete virtualDesktopManager;
    instance = nullptr;

    ::CoUninitialize();
}

bool WindowToTrayApp::Initialize(HINSTANCE hInst)
{
    hInstance = hInst;

    // COM
    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        ::MessageBox(nullptr, I18N::S("error_com_init"), I18N::S("already_running_title"), MB_OK | MB_ICONERROR);
        return false;
    }

    // Common Controls
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    ::InitCommonControlsEx(&icc);

    // Load and apply settings
    settingsMgr.Load();
    settings = settingsMgr.Get();
    I18N::SetLanguage(settings.language);
    WindowManager::SetUseVirtualDesktop(settings.useVirtualDesktop);

    // Initialize Virtual Desktop Manager
    virtualDesktopManager = new VirtualDesktopManager();
    virtualDesktopManager->Initialize();
    WindowManager::SetVirtualDesktopManager(virtualDesktopManager);

    // Register window class
    WNDCLASSEX wc{ sizeof(wc) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WindowToTrayClass";
    wc.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!::RegisterClassEx(&wc)) return false;

    // Create the hidden main window
    CreateMainWindow(hInstance);
    if (!mainWindow) return false;

    // Initialize Tray Manager and Hooks
    trayManager = new TrayManager(mainWindow);

    mouseHook = new GlobalHook();
    mouseHook->SetMouseCallback(
        [this](POINT pt, HWND hwnd) { OnMouseHook(pt, hwnd); });
    if (!mouseHook->Install())
    {
        ::MessageBox(nullptr, L"Failed to install global mouse hook!",
            I18N::S("already_running_title"), MB_OK | MB_ICONERROR);
        // Non-fatal, continue execution
    }

    CreateTrayIcon();

    // Apply settings (language, virtual desktop, hotkeys)
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
        nullptr, nullptr, hInst, nullptr);

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
    // Language
    I18N::SetLanguage(settings.language);

    // Virtual Desktop
    WindowManager::SetUseVirtualDesktop(settings.useVirtualDesktop);

    // Update main tray icon's tooltip (for multilingual support)
    if (mainTrayIconCreated) {
        wcscpy_s(mainTrayIcon.szTip, I18N::S("tray_tooltip"));
        mainTrayIcon.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE;
        ::Shell_NotifyIconW(NIM_MODIFY, &mainTrayIcon);
    }
}

void WindowToTrayApp::RegisterHotkeys()
{
    UnregisterHotkeys();
    bool ok1 = true, ok2 = true;

    if (settings.hkMinTop.vk) {
        ok1 = !!::RegisterHotKey(mainWindow, ID_HOTKEY_MIN_TOP, settings.hkMinTop.modifiers, settings.hkMinTop.vk);
    }
    if (settings.hkHideAll.vk) {
        ok2 = !!::RegisterHotKey(mainWindow, ID_HOTKEY_HIDE_ALL, settings.hkHideAll.modifiers, settings.hkHideAll.vk);
    }
    if (!ok1 || !ok2) {
        ::MessageBoxW(mainWindow, I18N::S("error_hotkey_reg_failed"), I18N::S("already_running_title"), MB_OK | MB_ICONWARNING);
    }
}
void WindowToTrayApp::UnregisterHotkeys()
{
    ::UnregisterHotKey(mainWindow, ID_HOTKEY_MIN_TOP);
    ::UnregisterHotKey(mainWindow, ID_HOTKEY_HIDE_ALL);
}

void WindowToTrayApp::OnHotkey(WPARAM hotkeyId)
{
    if (hotkeyId == ID_HOTKEY_MIN_TOP) {
        MinimizeTopWindow();
    }
    else if (hotkeyId == ID_HOTKEY_HIDE_ALL) {
        HideAllVisibleWindows();
    }
}

void WindowToTrayApp::MinimizeTopWindow()
{
    HWND tgt = ::GetForegroundWindow();

    // If the current foreground window is not valid, try the window under the cursor
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
        trayManager->AddWindowToTray(tgt, currentDesktop);
}

// Hides all windows that are actually visible on the current desktop
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

    // Collection phase: Collect first, then process in batch to avoid Z-order changes during enumeration
    ::EnumWindows([](HWND hwnd, LPARAM lp)->BOOL {
        auto& c = *reinterpret_cast<Ctx*>(lp);

        if (!::IsWindow(hwnd)) return TRUE;
        if (hwnd == c.app->mainWindow) return TRUE;
        if (!WindowManager::IsValidTargetWindow(hwnd)) return TRUE;
        if (c.app->trayManager->IsWindowInTray(hwnd)) return TRUE;
        if (::IsIconic(hwnd)) return TRUE;
        if (IsWindowCloaked(hwnd)) return TRUE;

        // If virtual desktops are available, the window must be on the current desktop
        if (c.currentDesktop >= 0 && c.app->virtualDesktopManager && c.app->virtualDesktopManager->IsAvailable()) {
            int wndDesk = c.app->virtualDesktopManager->GetWindowDesktopNumber(hwnd);
            if (wndDesk >= 0 && wndDesk != c.currentDesktop) return TRUE;
        }

        // Must intersect with a monitor's work area to avoid off-screen/background windows
        if (!IntersectsMonitorWorkArea(hwnd)) return TRUE;

        c.list->push_back(hwnd);
        return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

    // Execution phase
    for (HWND h : *ctx.list) {
        if (!::IsWindow(h)) continue;
        // Re-validate for safety
        if (!WindowManager::IsValidTargetWindow(h)) continue;
        if (WindowManager::MinimizeToTray(h)) {
            int originalDesktop = (currentDesktop >= 0) ? currentDesktop : 0;
            trayManager->AddWindowToTray(h, originalDesktop);
        }
    }

    delete ctx.list;
}

void WindowToTrayApp::OnMouseHook(POINT /*pt*/, HWND targetWindow)
{
    if (!WindowManager::IsValidTargetWindow(targetWindow)) return;

    // Let the UI thread execute the hiding logic
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
    return instance ?
        instance->HandleMessage(hwnd, msg, wParam, lParam) :
        ::DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT WindowToTrayApp::HandleMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        // Hide request from the hook thread
    case WM_MINIMIZE_TO_TRAY:
    {
        HWND tgt = reinterpret_cast<HWND>(wParam);

        int currentDesktop = 0;
        if (virtualDesktopManager && virtualDesktopManager->IsAvailable())
            currentDesktop = virtualDesktopManager->GetCurrentDesktopNumber();

        if (WindowManager::MinimizeToTray(tgt))
            trayManager->AddWindowToTray(tgt, currentDesktop);
        return 0;
    }

    // Global hotkey
    case WM_HOTKEY:
        OnHotkey(wParam);
        return 0;

        // Tray icon callback - handles clicks on all tray icons
    case WM_TRAY_CALLBACK:
    {
        UINT iconId = static_cast<UINT>(LOWORD(wParam));
        UINT eventCode = static_cast<UINT>(lParam);

        // Main tray icon (uID==1)
        if (iconId == 1) {
            // Right-click for context menu
            if (eventCode == WM_RBUTTONDOWN || eventCode == WM_RBUTTONUP || eventCode == WM_CONTEXTMENU) {
                POINT pt; ::GetCursorPos(&pt);
                HMENU m = ::CreatePopupMenu();
                ::AppendMenu(m, MF_STRING, IDM_RESTORE_ALL, I18N::S("menu_restore_all"));
                ::AppendMenu(m, MF_STRING, IDM_SETTINGS, I18N::S("menu_settings"));
                ::AppendMenu(m, MF_SEPARATOR, 0, nullptr);
                ::AppendMenu(m, MF_STRING, IDM_ABOUT, I18N::S("menu_about"));
                ::AppendMenu(m, MF_STRING, IDM_EXIT, I18N::S("menu_exit"));
                ::SetForegroundWindow(hwnd);
                ::TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                ::DestroyMenu(m);
                return 0;
            }
            return 0;
        }

        // Other tray icons (from windows) are handled by TrayManager
        trayManager->HandleTrayMessage(wParam, lParam);
        return 0;
    }

    // Main menu commands
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
            trayManager->RestoreAllWindows();
            if (virtualDesktopManager)
            {
                virtualDesktopManager->ForceRemoveHiddenDesktop();
            }
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

        // Automatically restore all windows and clean up on destroy
    case WM_DESTROY:
        trayManager->RestoreAllWindows();
        if (virtualDesktopManager)
        {
            virtualDesktopManager->ForceRemoveHiddenDesktop();
        }
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    EnableDpiAwareness();

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