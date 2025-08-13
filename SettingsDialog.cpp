#include "SettingsDialog.h"
#include <windowsx.h>
#include <cassert>

using namespace I18N;

static const wchar_t* kWndClass = L"WTT_SettingsDialog";

SettingsDialog::SettingsDialog(HWND parent, const Settings& current)
    : hParent_(parent)
    , hWnd_(nullptr)
    , cur_(current)
    , hChkUseVD_(nullptr)
    , hComboLang_(nullptr)
    , hHkMinTop_(nullptr)
    , hHkHideAll_(nullptr)
    , hBtnSave_(nullptr)
    , hBtnCancel_(nullptr) {
}

SettingsDialog::~SettingsDialog() {}

bool SettingsDialog::ShowModal(HWND parent,
    const Settings& current,
    Settings& outSettings)
{
    SettingsDialog dlg(parent, current);
    if (!dlg.Create()) {
        ::MessageBoxW(parent, L"Failed to create settings dialog!",
            L"Window-To-Tray", MB_OK | MB_ICONERROR);
        return false;
    }
    return dlg.DoModalLoop(outSettings) == IDOK;
}

bool SettingsDialog::Create()
{
    // Register window class
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = SettingsDialog::WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWndClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    // Init common controls
    INITCOMMONCONTROLSEX icc{ sizeof(icc),
                              ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // Get DPI and scale window size accordingly
    dpi_ = 96;
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using PFN_GetDpiForWindow = UINT(WINAPI*)(HWND);
        using PFN_GetDpiForSystem = UINT(WINAPI*)();
        auto pGetDpiForWindow = reinterpret_cast<PFN_GetDpiForWindow>(
            GetProcAddress(user32, "GetDpiForWindow"));
        auto pGetDpiForSystem = reinterpret_cast<PFN_GetDpiForSystem>(
            GetProcAddress(user32, "GetDpiForSystem"));
        if (pGetDpiForWindow && hParent_) {
            dpi_ = pGetDpiForWindow(hParent_);
        }
        else if (pGetDpiForSystem) {
            dpi_ = pGetDpiForSystem();
        }
    }
    auto Scale = [&](int v) { return MulDiv(v, (int)dpi_, 96); };

    // Base size at 96 DPI
    int baseW = 560;
    int baseH = 340;
    int winW = Scale(baseW);
    int winH = Scale(baseH);

    // Ensure the dialog fits on the screen
    HWND refWnd = (hParent_ && IsWindowVisible(hParent_)) ? hParent_ : nullptr;
    HMONITOR mon = MonitorFromWindow(refWnd ? refWnd : GetDesktopWindow(),
        MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    int workW = mi.rcWork.right - mi.rcWork.left;
    int workH = mi.rcWork.bottom - mi.rcWork.top;
    int margin = Scale(20);
    if (winW > workW - margin) winW = workW - margin;
    if (winH > workH - margin) winH = workH - margin;
    winW = max(winW, Scale(420));
    winH = max(winH, Scale(300));

    DWORD style = WS_CAPTION | WS_SYSMENU | WS_POPUPWINDOW;

    hWnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME,
        kWndClass,
        S("settings_title"),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winW, winH,
        hParent_, nullptr,
        wc.hInstance, this);

    if (!hWnd_) return false;

    BuildUi();
    ApplyLocalization();
    CenterToParent();
    ShowWindow(hWnd_, SW_SHOW);
    UpdateWindow(hWnd_);
    SetForegroundWindow(hWnd_);
    return true;
}


void SettingsDialog::Destroy()
{
    if (hWnd_) {
        DestroyWindow(hWnd_);
        hWnd_ = nullptr;
    }
}

INT_PTR SettingsDialog::DoModalLoop(Settings& outSettings)
{
    EnableWindow(hParent_, FALSE);

    MSG msg;
    while (IsWindow(hWnd_) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hWnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hParent_, TRUE);
    SetForegroundWindow(hParent_);

    if (saved_) {
        outSettings = result_;
        return IDOK;
    }
    return IDCANCEL;
}

void SettingsDialog::BuildUi()
{
    auto SX = [&](int v) { return MulDiv(v, (int)dpi_, 96); };

    RECT rc{}; GetClientRect(hWnd_, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;

    const int margin = SX(30);
    const int gap = SX(10);
    const int labelW = SX(240);
    const int rowH = SX(24);
    const int tipH = SX(40);
    const int btnW = SX(100);
    const int btnH = SX(30);

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Language
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        margin, SX(26), SX(120), SX(20),
        hWnd_, (HMENU)IDC_LABEL_LANG, nullptr, nullptr);

    int comboX = SX(170);
    int comboW = max(SX(180), cw - comboX - margin);
    hComboLang_ = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        comboX, SX(22), comboW, SX(140),
        hWnd_, (HMENU)IDC_COMBO_LANG,
        nullptr, nullptr);

    // Use Virtual Desktop Enhancement
    hChkUseVD_ = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        margin, SX(70), cw - 2 * margin, rowH,
        hWnd_, (HMENU)IDC_CHK_USEVD,
        nullptr, nullptr);

    // Hotkey: Minimize Top Window
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        margin, SX(116), labelW, SX(20),
        hWnd_, (HMENU)IDC_LABEL_HK_MIN, nullptr, nullptr);

    int hkW = SX(220);
    int hkX = cw - margin - hkW;
    hHkMinTop_ = CreateWindowExW(0, HOTKEY_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        hkX, SX(112), hkW, rowH,
        hWnd_, (HMENU)IDC_HOTKEY_MIN_TOP,
        nullptr, nullptr);

    // Hotkey: Hide All Windows
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        margin, SX(152), labelW, SX(20),
        hWnd_, (HMENU)IDC_LABEL_HK_HIDE_ALL, nullptr, nullptr);

    hHkHideAll_ = CreateWindowExW(0, HOTKEY_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        hkX, SX(148), hkW, rowH,
        hWnd_, (HMENU)IDC_HOTKEY_HIDE_ALL,
        nullptr, nullptr);

    // Hotkey Tip
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        margin, SX(190), cw - 2 * margin, tipH,
        hWnd_, (HMENU)IDC_LABEL_HK_TIP, nullptr, nullptr);

    // Buttons: Aligned to the bottom right
    int btnY = ch - margin - btnH;
    int btnCancelX = cw - margin - btnW;
    int btnSaveX = btnCancelX - gap - btnW;

    hBtnSave_ = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        btnSaveX, btnY, btnW, btnH,
        hWnd_, (HMENU)IDOK,
        nullptr, nullptr);

    hBtnCancel_ = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        btnCancelX, btnY, btnW, btnH,
        hWnd_, (HMENU)IDCANCEL,
        nullptr, nullptr);

    // Apply font to all controls
    auto setFont = [&](HWND h) {
        if (h) SendMessageW(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        };
    setFont(hComboLang_);  setFont(hChkUseVD_);
    setFont(hHkMinTop_);   setFont(hHkHideAll_);
    setFont(GetDlgItem(hWnd_, IDC_LABEL_LANG));
    setFont(GetDlgItem(hWnd_, IDC_LABEL_HK_MIN));
    setFont(GetDlgItem(hWnd_, IDC_LABEL_HK_HIDE_ALL));
    setFont(GetDlgItem(hWnd_, IDC_LABEL_HK_TIP));
    setFont(hBtnSave_); setFont(hBtnCancel_);

    // Set initial values
    SendMessageW(hComboLang_, CB_ADDSTRING, 0, (LPARAM)L"ÖÐÎÄ (Chinese)");
    SendMessageW(hComboLang_, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(hComboLang_, CB_SETCURSEL,
        (cur_.language == Language::English) ? 1 : 0, 0);

    SendMessageW(hChkUseVD_, BM_SETCHECK,
        cur_.useVirtualDesktop ? BST_CHECKED : BST_UNCHECKED, 0);

    SendMessageW(hHkMinTop_, HKM_SETHOTKEY,
        MakeHotkeyWord(cur_.hkMinTop), 0);
    SendMessageW(hHkHideAll_, HKM_SETHOTKEY,
        MakeHotkeyWord(cur_.hkHideAll), 0);
}
void SettingsDialog::ApplyLocalization()
{
    SetWindowTextW(hWnd_, S("settings_title"));
    SetWindowTextW(GetDlgItem(hWnd_, IDC_LABEL_LANG),
        S("settings_language"));
    SetWindowTextW(hChkUseVD_, S("settings_use_virtual_desktop"));
    SetWindowTextW(GetDlgItem(hWnd_, IDC_LABEL_HK_MIN),
        S("settings_hotkey_min_top"));
    SetWindowTextW(GetDlgItem(hWnd_, IDC_LABEL_HK_HIDE_ALL),
        S("settings_hotkey_hide_all"));
    SetWindowTextW(GetDlgItem(hWnd_, IDC_LABEL_HK_TIP),
        S("settings_hotkey_tip"));
    SetWindowTextW(hBtnSave_, S("settings_btn_save"));
    SetWindowTextW(hBtnCancel_, S("settings_btn_cancel"));
}

void SettingsDialog::CenterToParent()
{
    RECT rcW{}; GetWindowRect(hWnd_, &rcW);

    HWND refWnd = (hParent_ && IsWindowVisible(hParent_)) ? hParent_ : nullptr;
    HMONITOR mon = MonitorFromWindow(refWnd ? refWnd : hWnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);

    int w = rcW.right - rcW.left;
    int h = rcW.bottom - rcW.top;
    int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
    int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;

    SetWindowPos(hWnd_, nullptr, x, y, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER);
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    SettingsDialog* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SettingsDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)self);
        self->hWnd_ = hWnd;
    }
    else {
        self = reinterpret_cast<SettingsDialog*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(hWnd, msg, wParam, lParam)
        : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT SettingsDialog::HandleMessage(HWND,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDOK:
            OnSave();   break;
        case IDCANCEL:
            OnCancel(); break;
        default: break;
        }
        return 0;
    }
    case WM_CLOSE:
        OnCancel();
        return 0;
    }
    return DefWindowProcW(hWnd_, msg, wParam, lParam);
}

void SettingsDialog::OnSave()
{
    Settings s = cur_;

    s.language =
        (SendMessageW(hComboLang_, CB_GETCURSEL, 0, 0) == 1)
        ? Language::English
        : Language::Chinese;

    s.useVirtualDesktop =
        (SendMessageW(hChkUseVD_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    s.hkMinTop = FromHotkeyWord(
        (WORD)SendMessageW(hHkMinTop_, HKM_GETHOTKEY, 0, 0));
    s.hkHideAll = FromHotkeyWord(
        (WORD)SendMessageW(hHkHideAll_, HKM_GETHOTKEY, 0, 0));

    result_ = s;
    saved_ = true;
    Destroy();
}

void SettingsDialog::OnCancel()
{
    saved_ = false;
    Destroy();
}

WORD SettingsDialog::MakeHotkeyWord(const Hotkey& hk)
{
    BYTE flags = 0;
    if (hk.modifiers & MOD_SHIFT)   flags |= HOTKEYF_SHIFT;
    if (hk.modifiers & MOD_CONTROL) flags |= HOTKEYF_CONTROL;
    if (hk.modifiers & MOD_ALT)     flags |= HOTKEYF_ALT;
    return MAKEWORD((BYTE)hk.vk, flags);
}

Hotkey SettingsDialog::FromHotkeyWord(WORD w)
{
    Hotkey hk{ 0, 0 };
    hk.vk = LOBYTE(w);
    BYTE f = HIBYTE(w);
    if (f & HOTKEYF_SHIFT)   hk.modifiers |= MOD_SHIFT;
    if (f & HOTKEYF_CONTROL) hk.modifiers |= MOD_CONTROL;
    if (f & HOTKEYF_ALT)     hk.modifiers |= MOD_ALT;
    if (hk.vk == 0) hk.modifiers = 0; // No key, no modifiers
    return hk;
}