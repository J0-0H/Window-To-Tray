#include "CollectionWindow.h"
#include "Resource.h"
#include "Strings.h"
#include <windowsx.h>
#include <dwmapi.h> 
#include <uxtheme.h> 
#include <algorithm>
#include <vector>

#pragma comment(lib, "dwmapi.lib") 
#pragma comment(lib, "UxTheme.lib") 

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static COLORREF kClrWindowBg = RGB(249, 249, 249);
static COLORREF kClrText = RGB(20, 20, 20);
static COLORREF kClrRowAlt = RGB(244, 244, 244);
static COLORREF kClrRowSelected = RGB(234, 234, 234);

CollectionWindow* CollectionWindow::s_instance = nullptr;

bool CollectionWindow::Show(HWND parent, TrayManager& trayManager, PositioningMode mode, DisableModeCallback onDisable) {
    if (s_instance && s_instance->hWnd_ && IsWindow(s_instance->hWnd_)) {
        SetForegroundWindow(s_instance->hWnd_);
        return true;
    }
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }

    s_instance = new CollectionWindow(parent, trayManager, mode, onDisable);
    if (!s_instance->CreateAndShow()) {
        delete s_instance;
        s_instance = nullptr;
        return false;
    }
    return true;
}

CollectionWindow::CollectionWindow(HWND parent, TrayManager& trayManager, PositioningMode mode, DisableModeCallback onDisable)
    : hParent_(parent),
    hWnd_(nullptr),
    trayManager_(trayManager),
    positioningMode_(mode),
    disableModeCallback_(onDisable),
    hListView_(nullptr),
    hBtnRestoreAll_(nullptr),
    hBtnToggleMode_(nullptr),
    hImageList_(nullptr)
{
}

CollectionWindow::~CollectionWindow() {
    if (hImageList_) {
        ImageList_Destroy(hImageList_);
        hImageList_ = nullptr;
    }
    if (hEventHook_) {
        UnhookWinEvent(hEventHook_);
        hEventHook_ = nullptr;
    }
    s_instance = nullptr; 
}

bool CollectionWindow::CreateAndShow() {
    const wchar_t* WND_CLASS = L"WTT_CollectionWindow";
    WNDCLASSEXW wc{ sizeof(wc) };
    if (!GetClassInfoExW(GetModuleHandle(nullptr), WND_CLASS, &wc)) {
        wc.lpfnWndProc = CollectionWindow::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = WND_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&wc);
    }

    dpi_ = GetDpiForWindow(hParent_);
    auto Scale = [&](int v) { return MulDiv(v, dpi_, 96); };

    int winW = Scale(420);
    int winH = Scale(500);

    DWORD style = WS_POPUP | WS_BORDER | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    hWnd_ = CreateWindowExW(exStyle, WND_CLASS, I18N::S("collection_window_title"), style,
        0, 0, winW, winH,
        hParent_, nullptr, GetModuleHandle(nullptr), this);

    if (!hWnd_) return false;

    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
    ::DwmSetWindowAttribute(hWnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
    int backdrop = DWMSBT_MAINWINDOW;
    ::DwmSetWindowAttribute(hWnd_, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

    BuildUI();
    PopulateList();

    AdjustWindowToContent();

    PositionWindow();

    hEventHook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, &CollectionWindow::WinEventProc,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    ShowWindow(hWnd_, SW_SHOW);
    SetForegroundWindow(hWnd_);
    SetFocus(hListView_);

    return true;
}

void CollectionWindow::Destroy() {
    if (closing_) return;
    closing_ = true;
    if (hEventHook_) {
        UnhookWinEvent(hEventHook_);
        hEventHook_ = nullptr;
    }
    if (hWnd_) {
        DestroyWindow(hWnd_);
        hWnd_ = nullptr;
    }
}

void CollectionWindow::PositionWindow() {
    RECT rc{};
    GetWindowRect(hWnd_, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    int x, y;

    if (positioningMode_ == PositioningMode::NearTray) {
        POINT pt;
        GetCursorPos(&pt);
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfoW(mon, &mi);

        x = mi.rcWork.right - w;
        y = mi.rcWork.bottom - h;

        if (pt.x > x && pt.y > y) {
            x = pt.x - w;
            y = pt.y - h;
        }
        x = max(mi.rcWork.left, min(x, mi.rcWork.right - w));
        y = max(mi.rcWork.top, min(y, mi.rcWork.bottom - h));

    }
    else { // CenterScreen
        HMONITOR mon = MonitorFromWindow(hParent_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfoW(mon, &mi);
        x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
        y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
    }

    SetWindowPos(hWnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void CollectionWindow::BuildUI() {
    auto Scale = [&](int v) { return MulDiv(v, dpi_, 96); };
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    RECT rcClient{};
    GetClientRect(hWnd_, &rcClient);

    margin_ = Scale(12);
    btnH_ = Scale(36); 
    btnRestoreW_ = Scale(120);
    btnToggleW_ = Scale(160); 

    hBtnToggleMode_ = CreateWindowExW(0, L"BUTTON", I18N::S("collection_disable_mode_button"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        margin_, rcClient.bottom - margin_ - btnH_,
        btnToggleW_, btnH_,
        hWnd_, (HMENU)IDC_BTN_TOGGLE_COLLECTION_MODE, GetModuleHandle(nullptr), nullptr);
    SendMessageW(hBtnToggleMode_, WM_SETFONT, (WPARAM)hFont, TRUE);
    ::SetWindowTheme(hBtnToggleMode_, L"Explorer", nullptr);


    hBtnRestoreAll_ = CreateWindowExW(0, L"BUTTON", I18N::S("menu_restore_all"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        rcClient.right - margin_ - btnRestoreW_, rcClient.bottom - margin_ - btnH_,
        btnRestoreW_, btnH_,
        hWnd_, (HMENU)IDC_BTN_RESTORE_ALL_COLLECTION, GetModuleHandle(nullptr), nullptr);
    SendMessageW(hBtnRestoreAll_, WM_SETFONT, (WPARAM)hFont, TRUE);
    ::SetWindowTheme(hBtnRestoreAll_, L"Explorer", nullptr);


    int listH = rcClient.bottom - (margin_ * 2) - btnH_ - Scale(8);
    hListView_ = CreateWindowExW(0, WC_LISTVIEW, L"", 
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER,
        margin_, margin_,
        rcClient.right - (margin_ * 2), listH,
        hWnd_, (HMENU)IDC_LIST_WINDOWS, GetModuleHandle(nullptr), nullptr);

    ListView_SetExtendedListViewStyle(hListView_, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
    ListView_SetBkColor(hListView_, kClrWindowBg); 
    SendMessageW(hListView_, WM_SETFONT, (WPARAM)hFont, TRUE);
    ::SetWindowTheme(hListView_, L"Explorer", nullptr);

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_WIDTH;
    RECT lvRect{};
    GetClientRect(hListView_, &lvRect);
    lvc.cx = lvRect.right - lvRect.left;
    ListView_InsertColumn(hListView_, 0, &lvc);
}

void CollectionWindow::PopulateList() {
    iconSize_ = GetSystemMetrics(SM_CYSMICON) + MulDiv(12, dpi_, 96);
    if (iconSize_ < 20) iconSize_ = 20;
    hImageList_ = ImageList_Create(iconSize_, iconSize_, ILC_COLOR32 | ILC_MASK, 10, 10);

    const auto& icons = trayManager_.GetTrayIcons();
    if (icons.empty()) {
        EnableWindow(hBtnRestoreAll_, FALSE);
    }
    else {
        EnableWindow(hBtnRestoreAll_, TRUE);
    }

    int imageIndex = 0;
    for (const auto& pair : icons) {
        const TrayIcon& ti = pair.second;
        if (ti.originalIcon) {
            ImageList_AddIcon(hImageList_, ti.originalIcon);
        }
        else {
            HICON hPlaceholder = (HICON)LoadImage(nullptr, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
            ImageList_AddIcon(hImageList_, hPlaceholder);
        }

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        lvi.iItem = imageIndex;
        lvi.iSubItem = 0;
        lvi.iImage = imageIndex;
        lvi.lParam = (LPARAM)pair.first;
        lvi.pszText = (LPWSTR)ti.windowTitle.c_str();

        ListView_InsertItem(hListView_, &lvi);
        imageIndex++;
    }

    ListView_SetImageList(hListView_, hImageList_, LVSIL_SMALL);
}

void CollectionWindow::AdjustWindowToContent() {
    const auto& icons = trayManager_.GetTrayIcons();
    int count = (int)icons.size();
    if (count < 0) count = 0;

    // --- MODIFIED: Increased per-row padding for a more spacious look ---
    int perRow = iconSize_ + MulDiv(12, dpi_, 96); 
    if (perRow < MulDiv(22, dpi_, 96)) perRow = MulDiv(22, dpi_, 96);

    int baseClientHeightNoList = margin_ + MulDiv(8, dpi_, 96) + btnH_ + margin_;

    HMONITOR mon = MonitorFromWindow(hParent_ ? hParent_ : hWnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    int workH = mi.rcWork.bottom - mi.rcWork.top;
    int maxClientH = (int)(workH * 2 / 3.0);
    int minRows = 3;
    int visibleRows = (count == 0) ? 1 : count;
    visibleRows = max(visibleRows, minRows);
    int desiredListH = perRow * visibleRows;

    int desiredClientH = baseClientHeightNoList + desiredListH;
    desiredClientH = min(desiredClientH, maxClientH);

    RECT rcW{}, rcC{};
    GetWindowRect(hWnd_, &rcW);
    GetClientRect(hWnd_, &rcC);
    int ncH = (rcW.bottom - rcW.top) - (rcC.bottom - rcC.top);
    int ncW = (rcW.right - rcW.left) - (rcC.right - rcC.left);
    int newWinH = desiredClientH + ncH;

    int winW = rcW.right - rcW.left;

    SetWindowPos(hWnd_, nullptr, 0, 0, winW, newWinH, SWP_NOMOVE | SWP_NOZORDER);
    UpdateLayout();
}

void CollectionWindow::UpdateLayout() {
    RECT rc{};
    GetClientRect(hWnd_, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;

    int listH = ch - (margin_ * 2) - btnH_ - MulDiv(8, dpi_, 96);
    if (listH < MulDiv(60, dpi_, 96)) listH = MulDiv(60, dpi_, 96);

    MoveWindow(hListView_, margin_, margin_, cw - (margin_ * 2), listH, TRUE);

    int btnY = ch - margin_ - btnH_;
    MoveWindow(hBtnRestoreAll_, cw - margin_ - btnRestoreW_, btnY, btnRestoreW_, btnH_, TRUE);
    MoveWindow(hBtnToggleMode_, margin_, btnY, btnToggleW_, btnH_, TRUE);

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_WIDTH;
    lvc.cx = cw - (margin_ * 2);
    ListView_SetColumnWidth(hListView_, 0, lvc.cx);
}

void CollectionWindow::OnNotify(LPARAM lParam) {
    LPNMHDR nmhdr = (LPNMHDR)lParam;
    if (nmhdr->hwndFrom == hListView_) {
        switch (nmhdr->code) {
        case LVN_ITEMACTIVATE: {
            LPNMITEMACTIVATE nmia = (LPNMITEMACTIVATE)lParam;
            if (nmia->iItem != -1) {
                OnRestoreItem(nmia->iItem);
            }
            return;
        }
        case NM_CUSTOMDRAW: {
            LPNMLVCUSTOMDRAW lvcd = (LPNMLVCUSTOMDRAW)lParam;
            switch (lvcd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                SetWindowLongPtr(hWnd_, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                return;
            case CDDS_ITEMPREPAINT:
                if (lvcd->nmcd.uItemState & CDIS_SELECTED) {
                    lvcd->clrTextBk = kClrRowSelected;
                }
                else {
                    if (lvcd->nmcd.dwItemSpec % 2 == 1) {
                        lvcd->clrTextBk = kClrRowAlt;
                    }
                    else {
                        lvcd->clrTextBk = kClrWindowBg;
                    }
                }
                lvcd->clrText = kClrText;
                SetWindowLongPtr(hWnd_, DWLP_MSGRESULT, CDRF_DODEFAULT);
                return;
            }
        }
        }
    }
}

void CollectionWindow::OnRestoreItem(int itemIndex) {
    LVITEMW lvi{};
    lvi.mask = LVIF_PARAM;
    lvi.iItem = itemIndex;
    if (ListView_GetItem(hListView_, &lvi)) {
        UINT iconId = (UINT)lvi.lParam;

        HWND hwndToRestore = nullptr;
        const auto& icons = trayManager_.GetTrayIcons();
        auto it = icons.find(iconId);
        if (it != icons.end()) {
            hwndToRestore = it->second.targetWindow;
        }
        lastRestoredHwnd_ = hwndToRestore;
        suppressCloseUntil_ = GetTickCount64() + 800; 

        trayManager_.RestoreWindowFromTray(iconId);

        ListView_DeleteItem(hListView_, itemIndex);

        if (ListView_GetItemCount(hListView_) == 0) {
            Destroy();
        }
        else {
            AdjustWindowToContent(); 
        }
    }
}

void CollectionWindow::OnRestoreAll() {
    trayManager_.RestoreAllWindows();
    Destroy();
}

void CollectionWindow::OnToggleCollectionMode() {
    if (disableModeCallback_) {
        disableModeCallback_();
    }
    Destroy();
}

void CollectionWindow::OnKillFocus() {
    HWND newFocus = GetFocus(); 
    HWND fg = GetForegroundWindow();
    if (ShouldDismissOnDeactivate(fg)) {
        Destroy();
    }
}

void CALLBACK CollectionWindow::WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (event != EVENT_SYSTEM_FOREGROUND) return;
    if (!s_instance || !IsWindow(s_instance->hWnd_)) return;
    s_instance->OnForegroundChanged(hwnd);
}

void CollectionWindow::OnForegroundChanged(HWND newForeground) {
    if (!IsWindow(hWnd_)) return;

    ULONGLONG now = GetTickCount64();
    if (now < suppressCloseUntil_ && newForeground && newForeground == lastRestoredHwnd_) {
        return;
    }

    if (ShouldDismissOnDeactivate(newForeground)) {
        Destroy();
    }
}

bool CollectionWindow::ShouldDismissOnDeactivate(HWND newForeground) const {
    if (!IsWindow(hWnd_)) return true;

    if (newForeground == hWnd_ || (newForeground && IsChild(hWnd_, newForeground))) {
        return false;
    }

    ULONGLONG now = GetTickCount64();
    if (now < suppressCloseUntil_ && newForeground && newForeground == lastRestoredHwnd_) {
        return false;
    }

    POINT pt{};
    GetCursorPos(&pt);
    RECT rc{};
    GetWindowRect(hWnd_, &rc);
    if (!PtInRect(&rc, pt)) {
        return true; 
    }

    return false;
}

LRESULT CALLBACK CollectionWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CollectionWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<CollectionWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)self);
        self->hWnd_ = hWnd;
    }
    else {
        self = reinterpret_cast<CollectionWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    return self ? self->HandleMessage(hWnd, msg, wParam, lParam)
        : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CollectionWindow::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd_, &rc);
        HBRUSH hBrush = CreateSolidBrush(kClrWindowBg);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        return 1;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_RESTORE_ALL_COLLECTION:
            OnRestoreAll();
            break;
        case IDC_BTN_TOGGLE_COLLECTION_MODE:
            OnToggleCollectionMode();
            break;
        }
        return 0;

    case WM_NOTIFY:
        OnNotify(lParam);
        return 0;

    case WM_KILLFOCUS:
        OnKillFocus();
        return 0;

    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE) {
            HWND newFg = GetForegroundWindow();
            if (ShouldDismissOnDeactivate(newFg)) {
                Destroy();
            }
        }
        return 0;

    case WM_CLOSE:
        Destroy();
        return 0;

    case WM_NCDESTROY:
        delete this;
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}