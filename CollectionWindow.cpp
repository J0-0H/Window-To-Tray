#include "CollectionWindow.h"
#include "Resource.h"
#include "Strings.h"
#include <windowsx.h>

CollectionWindow* CollectionWindow::s_instance = nullptr;

bool CollectionWindow::Show(HWND parent, TrayManager& trayManager, PositioningMode mode, DisableModeCallback onDisable) {
    // 如果窗口已存在，则将其带到前台并返回
    if (s_instance && s_instance->hWnd_ && IsWindow(s_instance->hWnd_)) {
        SetForegroundWindow(s_instance->hWnd_);
        return true;
    }
    // 销毁任何可能残留的旧实例
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
    }
    s_instance = nullptr; // 清除静态实例指针
}

bool CollectionWindow::CreateAndShow() {
    // 注册窗口类
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

    // Powertoys-like size
    int winW = Scale(420);
    int winH = Scale(500);

    DWORD style = WS_POPUP | WS_BORDER | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    hWnd_ = CreateWindowExW(exStyle, WND_CLASS, I18N::S("collection_window_title"), style,
        0, 0, winW, winH,
        hParent_, nullptr, GetModuleHandle(nullptr), this);

    if (!hWnd_) return false;

    BuildUI();
    PopulateList();
    PositionWindow();

    ShowWindow(hWnd_, SW_SHOW);
    SetForegroundWindow(hWnd_);
    SetFocus(hListView_);

    return true;
}

void CollectionWindow::Destroy() {
    if (hWnd_) {
        DestroyWindow(hWnd_);
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

        // 默认放在右下角
        x = mi.rcWork.right - w;
        y = mi.rcWork.bottom - h;

        // 尝试调整，避免光标被遮挡
        if (pt.x > x && pt.y > y) {
            x = pt.x - w;
            y = pt.y - h;
        }
        // 确保在工作区内
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

    const int margin = Scale(12);
    const int btnH = Scale(32);
    const int btnRestoreW = Scale(120);
    const int btnToggleW = Scale(140);

    // "Toggle Collection Mode" Button
    hBtnToggleMode_ = CreateWindowExW(0, L"BUTTON", I18N::S("collection_disable_mode_button"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        margin, rcClient.bottom - margin - btnH,
        btnToggleW, btnH,
        hWnd_, (HMENU)IDC_BTN_TOGGLE_COLLECTION_MODE, GetModuleHandle(nullptr), nullptr);
    SendMessageW(hBtnToggleMode_, WM_SETFONT, (WPARAM)hFont, TRUE);

    // "Restore All" Button
    hBtnRestoreAll_ = CreateWindowExW(0, L"BUTTON", I18N::S("menu_restore_all"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        rcClient.right - margin - btnRestoreW, rcClient.bottom - margin - btnH,
        btnRestoreW, btnH,
        hWnd_, (HMENU)IDC_BTN_RESTORE_ALL_COLLECTION, GetModuleHandle(nullptr), nullptr);
    SendMessageW(hBtnRestoreAll_, WM_SETFONT, (WPARAM)hFont, TRUE);

    // --- MODIFIED: Change ListView style to LVS_REPORT ---
    int listH = rcClient.bottom - (margin * 2) - btnH - Scale(8);
    hListView_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER, // Use LVS_REPORT and hide header
        margin, margin,
        rcClient.right - (margin * 2), listH,
        hWnd_, (HMENU)IDC_LIST_WINDOWS, GetModuleHandle(nullptr), nullptr);

    ListView_SetExtendedListViewStyle(hListView_, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
    SendMessageW(hListView_, WM_SETFONT, (WPARAM)hFont, TRUE);

    // --- NEW: Add a single column that fills the entire width ---
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_WIDTH;
    RECT lvRect{};
    GetClientRect(hListView_, &lvRect);
    lvc.cx = lvRect.right - lvRect.left; // Set column width to fill the control
    ListView_InsertColumn(hListView_, 0, &lvc);
}

void CollectionWindow::PopulateList() {
    // --- MODIFIED: Use small icon size for list view ---
    const int iconSize = GetSystemMetrics(SM_CXSMICON);
    hImageList_ = ImageList_Create(iconSize, iconSize, ILC_COLOR32 | ILC_MASK, 10, 10);

    const auto& icons = trayManager_.GetTrayIcons();
    if (icons.empty()) {
        EnableWindow(hBtnRestoreAll_, FALSE);
        return;
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
        lvi.iItem = imageIndex; // Row index
        lvi.iSubItem = 0;       // Column index
        lvi.iImage = imageIndex;
        lvi.lParam = (LPARAM)pair.first; // Store the icon ID
        lvi.pszText = (LPWSTR)ti.windowTitle.c_str();

        ListView_InsertItem(hListView_, &lvi);
        imageIndex++;
    }

    // --- MODIFIED: Set the SMALL image list for LVS_REPORT view ---
    ListView_SetImageList(hListView_, hImageList_, LVSIL_SMALL);
}

void CollectionWindow::OnNotify(LPARAM lParam) {
    LPNMHDR nmhdr = (LPNMHDR)lParam;
    if (nmhdr->hwndFrom == hListView_ && nmhdr->code == LVN_ITEMACTIVATE) {
        LPNMITEMACTIVATE nmia = (LPNMITEMACTIVATE)lParam;
        if (nmia->iItem != -1) {
            OnRestoreItem(nmia->iItem);
        }
    }
}

void CollectionWindow::OnRestoreItem(int itemIndex) {
    LVITEMW lvi{};
    lvi.mask = LVIF_PARAM;
    lvi.iItem = itemIndex;
    if (ListView_GetItem(hListView_, &lvi)) {
        UINT iconId = (UINT)lvi.lParam;
        trayManager_.RestoreWindowFromTray(iconId);

        // --- MODIFIED: Don't close the window, just remove the item from the list. ---
        ListView_DeleteItem(hListView_, itemIndex);

        // If the list is now empty, close the window.
        if (ListView_GetItemCount(hListView_) == 0) {
            Destroy();
        }
    }
}

void CollectionWindow::OnRestoreAll() {
    trayManager_.RestoreAllWindows();
    Destroy(); // Close after restoring
}

void CollectionWindow::OnToggleCollectionMode() {
    if (disableModeCallback_) {
        disableModeCallback_();
    }
    Destroy(); // Close after the action
}

void CollectionWindow::OnKillFocus() {
    HWND hFocus = GetFocus();
    // Only destroy if focus moves to a window that is not this window or a child of it.
    if (hFocus != hWnd_ && !IsChild(hWnd_, hFocus)) {
        Destroy();
    }
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

    case WM_CLOSE:
        Destroy();
        return 0;

    case WM_NCDESTROY:
        // This is the last message a window receives.
        // It's the proper place to delete the C++ object.
        delete this;
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}