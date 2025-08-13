#include "TrayManager.h"
#include "Resource.h"
#include "WindowManager.h"
#include "VirtualDesktopManager.h"
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <propsys.h>
#include <propvarutil.h>
#include <propkey.h>
#include <gdiplus.h>
#include <psapi.h>
#include "UwpIconUtils.h"
#include "Strings.h"
#include <vector>
#include <queue>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")

using namespace Gdiplus;
using namespace I18N;


static bool EnsureGdiPlus() {
    static bool       inited = false;
    static ULONG_PTR  token = 0;
    if (!inited) {
        GdiplusStartupInput si;
        if (GdiplusStartup(&token, &si, nullptr) == Ok)
            inited = true;
    }
    return inited;
}

// Converts a PNG file to an HICON
static HICON LoadPngAsIcon(const std::wstring& path, int size) {
    if (!EnsureGdiPlus()) return nullptr;
    Bitmap src(path.c_str());
    if (src.GetLastStatus() != Ok) return nullptr;
    Bitmap dst(size, size, PixelFormat32bppARGB);
    Graphics g(&dst);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(&src, 0, 0, size, size);
    HBITMAP hbmColor = nullptr;
    if (dst.GetHBITMAP(Color(0, 0, 0, 0), &hbmColor) != Ok) return nullptr;
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = CreateBitmap(size, size, 1, 1, nullptr);
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
    return hIcon;
}

/**
 * @brief Defines the bounding box of the non-transparent content.
 */
struct ContentBounds {
    int left, top, right, bottom;
    bool isEmpty;

    ContentBounds() : left(INT_MAX), top(INT_MAX), right(-1), bottom(-1), isEmpty(true) {}

    void addPoint(int x, int y) {
        if (isEmpty) {
            left = right = x;
            top = bottom = y;
            isEmpty = false;
        }
        else {
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }

    int width() const { return isEmpty ? 0 : (right - left + 1); }
    int height() const { return isEmpty ? 0 : (bottom - top + 1); }

    // Calculates the minimum distance from the content area to the icon's edge.
    int getMinDistanceToEdge(int iconWidth, int iconHeight) const {
        if (isEmpty) return 0;
        return std::min({ left, top, iconWidth - 1 - right, iconHeight - 1 - bottom });
    }
};

/**
 * @brief Smartly removes the icon background and scales the content area proportionally.
 */
HICON RemoveIconBackground_FloodFillAndScale(HICON hIcon, bool& outOwnsNewIcon)
{
    outOwnsNewIcon = false;
    if (!hIcon) return nullptr;

    ICONINFO info{};
    if (!::GetIconInfo(hIcon, &info)) return nullptr;

    auto cleanup = [&]() {
        if (info.hbmColor) ::DeleteObject(info.hbmColor);
        if (info.hbmMask) ::DeleteObject(info.hbmMask);
        };

    if (!info.hbmColor) {
        cleanup();
        return nullptr;
    }

    BITMAP bm;
    if (::GetObject(info.hbmColor, sizeof(bm), &bm) == 0 || bm.bmBitsPixel != 32) {
        cleanup();
        return nullptr;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bm.bmWidth;
    bi.bmiHeader.biHeight = -bm.bmHeight;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    std::vector<DWORD> pixels(bm.bmWidth * bm.bmHeight);
    HDC hdc = ::GetDC(nullptr);
    if (!::GetDIBits(hdc, info.hbmColor, 0, bm.bmHeight, pixels.data(), &bi, DIB_RGB_COLORS)) {
        ::ReleaseDC(nullptr, hdc);
        cleanup();
        return nullptr;
    }
    ::ReleaseDC(nullptr, hdc);

    // Step 1: Use Flood Fill to remove the background
    DWORD bgColor = pixels[0];
    if ((bgColor >> 24) != 0) { // Only process if the background is not transparent
        std::queue<POINT> q;
        auto AddSeed = [&](int x, int y) {
            if (x >= 0 && x < bm.bmWidth && y >= 0 && y < bm.bmHeight) {
                DWORD& pixel = pixels[y * bm.bmWidth + x];
                if (pixel == bgColor) {
                    q.push({ x, y });
                    pixel = 0; // Make transparent
                }
            }
            };

        // Start filling from the four corners
        AddSeed(0, 0);
        AddSeed(bm.bmWidth - 1, 0);
        AddSeed(0, bm.bmHeight - 1);
        AddSeed(bm.bmWidth - 1, bm.bmHeight - 1);

        while (!q.empty()) {
            POINT p = q.front();
            q.pop();

            AddSeed(p.x + 1, p.y);
            AddSeed(p.x - 1, p.y);
            AddSeed(p.x, p.y + 1);
            AddSeed(p.x, p.y - 1);
        }
    }

    // Step 2: Analyze content bounds
    ContentBounds bounds;
    for (int y = 0; y < bm.bmHeight; y++) {
        for (int x = 0; x < bm.bmWidth; x++) {
            if ((pixels[y * bm.bmWidth + x] >> 24) > 16) { // Alpha > 16
                bounds.addPoint(x, y);
            }
        }
    }

    if (bounds.isEmpty) {
        cleanup();
        outOwnsNewIcon = true;
        return CopyIcon(hIcon);
    }

    // Step 3: Calculate scaling parameters
    int contentWidth = bounds.width();
    int contentHeight = bounds.height();
    int minDistance = bounds.getMinDistanceToEdge(bm.bmWidth, bm.bmHeight);

    double maxScaleX = (double)bm.bmWidth / contentWidth;
    double maxScaleY = (double)bm.bmHeight / contentHeight;
    double scale = std::min(maxScaleX, maxScaleY);
    scale = std::min(scale, 3.0); // Limit max scale
    scale = std::max(scale, 1.0); // Ensure it doesn't shrink
    if (minDistance < 4) { // If content is already close to the edge, scale less
        scale = std::min(scale, 1.2);
    }

    // Step 4: Create the scaled image if needed
    if (scale > 1.01) {
        std::vector<DWORD> scaledPixels(bm.bmWidth * bm.bmHeight, 0);
        int newContentWidth = (int)(contentWidth * scale);
        int newContentHeight = (int)(contentHeight * scale);

        int offsetX = (bm.bmWidth - newContentWidth) / 2;
        int offsetY = (bm.bmHeight - newContentHeight) / 2;

        for (int y = 0; y < newContentHeight && y + offsetY < bm.bmHeight; y++) {
            for (int x = 0; x < newContentWidth && x + offsetX < bm.bmWidth; x++) {
                int srcX = bounds.left + (int)((double)x / scale);
                int srcY = bounds.top + (int)((double)y / scale);

                if (srcX >= bounds.left && srcX <= bounds.right &&
                    srcY >= bounds.top && srcY <= bounds.bottom &&
                    srcX < bm.bmWidth && srcY < bm.bmHeight) {

                    DWORD srcPixel = pixels[srcY * bm.bmWidth + srcX];
                    if ((srcPixel >> 24) > 16) {
                        int dstX = x + offsetX;
                        int dstY = y + offsetY;
                        if (dstX >= 0 && dstX < bm.bmWidth && dstY >= 0 && dstY < bm.bmHeight) {
                            scaledPixels[dstY * bm.bmWidth + dstX] = srcPixel;
                        }
                    }
                }
            }
        }
        pixels = scaledPixels;
    }

    // Step 5: Create the new icon
    HBITMAP hNewBitmapColor = ::CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 32, pixels.data());
    if (!hNewBitmapColor) {
        cleanup();
        return nullptr;
    }

    HBITMAP hNewBitmapMask = ::CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, nullptr);
    if (!hNewBitmapMask) {
        ::DeleteObject(hNewBitmapColor);
        cleanup();
        return nullptr;
    }

    ICONINFO newIconInfo{};
    newIconInfo.fIcon = TRUE;
    newIconInfo.hbmColor = hNewBitmapColor;
    newIconInfo.hbmMask = hNewBitmapMask;
    HICON hNewIcon = ::CreateIconIndirect(&newIconInfo);

    ::DeleteObject(hNewBitmapColor);
    ::DeleteObject(hNewBitmapMask);
    cleanup();

    if (hNewIcon) {
        outOwnsNewIcon = true;
    }
    return hNewIcon;
}


TrayManager::TrayManager(HWND mainWindow)
    : mainWindow(mainWindow), nextIconId(1000) {
}

TrayManager::~TrayManager() { RemoveAllTrayIcons(); }

bool TrayManager::AddWindowToTray(HWND hwnd, int originalDesktop)
{
    if (!IsWindow(hwnd)) return false;
    for (auto& p : trayIcons)
        if (p.second.targetWindow == hwnd) return false;

    TrayIcon ti{};
    ti.targetWindow = hwnd;
    ti.windowTitle = GetWindowTitle(hwnd);
    ti.originalIcon = GetWindowIcon(hwnd, ti.ownsIcon);
    ti.wasMaximized = ::IsZoomed(hwnd) ? true : false;
    ti.originalDesktop = originalDesktop;

    ti.nid.cbSize = sizeof(NOTIFYICONDATA);
    ti.nid.hWnd = mainWindow;
    ti.nid.uID = nextIconId;
    ti.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    ti.nid.uCallbackMessage = WM_TRAY_CALLBACK;
    ti.nid.hIcon = ti.originalIcon;
    ::wcscpy_s(ti.nid.szTip, ti.windowTitle.c_str());

    if (::Shell_NotifyIconW(NIM_ADD, &ti.nid)) {
        trayIcons[nextIconId++] = ti;
        return true;
    }
    if (ti.ownsIcon && ti.originalIcon) ::DestroyIcon(ti.originalIcon);
    return false;
}

bool TrayManager::RestoreWindowFromTray(UINT iconId)
{
    auto it = trayIcons.find(iconId);
    if (it == trayIcons.end()) return false;

    HWND hwnd = it->second.targetWindow;
    if (IsWindow(hwnd)) {
        WindowManager::ShowWindowInTaskbar(hwnd, it->second.originalDesktop);
        ::ShowWindow(hwnd,
            it->second.wasMaximized ? SW_SHOWMAXIMIZED : SW_RESTORE);
        ::SetForegroundWindow(hwnd);
    }

    ::Shell_NotifyIconW(NIM_DELETE, &it->second.nid);
    if (it->second.ownsIcon && it->second.originalIcon)
        ::DestroyIcon(it->second.originalIcon);
    trayIcons.erase(it);
    return true;
}

void TrayManager::RestoreAllWindows()
{
    auto copy = trayIcons;
    for (auto& p : copy) RestoreWindowFromTray(p.first);
}

void TrayManager::RemoveAllTrayIcons()
{
    for (auto& p : trayIcons) {
        ::Shell_NotifyIconW(NIM_DELETE, &p.second.nid);
        if (p.second.ownsIcon && p.second.originalIcon)
            ::DestroyIcon(p.second.originalIcon);
    }
    trayIcons.clear();
}

bool TrayManager::HandleTrayMessage(WPARAM wParam, LPARAM lParam)
{
    UINT id = static_cast<UINT>(wParam);
    UINT msg = LOWORD(lParam);

    switch (msg) {
    case WM_LBUTTONDOWN: return RestoreWindowFromTray(id);
    case WM_RBUTTONDOWN:
        POINT pt; ::GetCursorPos(&pt);
        ShowTrayMenu(mainWindow, pt);
        return true;
    }
    return false;
}

bool TrayManager::IsWindowInTray(HWND hwnd) const
{
    for (const auto& kv : trayIcons) {
        if (kv.second.targetWindow == hwnd) return true;
    }
    return false;
}

// Unified function to get a window's icon, with special handling for UWP apps.
HICON TrayManager::GetWindowIcon(HWND hwnd, bool& owns)
{
    owns = false;

    // 1) UWP / Packaged Apps
    {
        int   sz = ::GetSystemMetrics(SM_CXICON);
        HICON hUwp = nullptr;
        bool  uwpOwns = false;

        if (UwpIconUtils::GetUwpWindowIcon(hwnd, sz, hUwp, uwpOwns) && hUwp)
        {
            bool processedIconNeedsOwning = false;
            // Call the new Flood Fill + Scaling function
            HICON hProcessedIcon = RemoveIconBackground_FloodFillAndScale(hUwp, processedIconNeedsOwning);

            if (hProcessedIcon) {
                if (uwpOwns) {
                    ::DestroyIcon(hUwp);
                }
                owns = processedIconNeedsOwning;
                return hProcessedIcon;
            }

            // Fallback to the original UWP icon if processing fails
            owns = uwpOwns;
            return hUwp;
        }
    }

    // 2) WM_GETICON / Class Icon
    HICON ico = (HICON)::SendMessage(hwnd, WM_GETICON, ICON_SMALL2, 0);
    if (!ico) ico = (HICON)::SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!ico) ico = (HICON)::SendMessage(hwnd, WM_GETICON, ICON_BIG, 0);
    if (!ico) ico = (HICON)::GetClassLongPtr(hwnd, GCLP_HICONSM);
    if (!ico) ico = (HICON)::GetClassLongPtr(hwnd, GCLP_HICON);
    if (ico)
    {
        return ico;
    }

    // 3) RelaunchIconResource / PNG from Property Store
    IPropertyStore* store = nullptr;
    if (SUCCEEDED(::SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store))))
    {
        PROPVARIANT var; PropVariantInit(&var);
        if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_RelaunchIconResource, &var)) &&
            var.vt == VT_LPWSTR && var.pwszVal && *var.pwszVal)
        {
            wchar_t resolved[MAX_PATH]{};
            if (SUCCEEDED(::SHLoadIndirectString(var.pwszVal, resolved, ARRAYSIZE(resolved), nullptr)) &&
                ::PathFileExistsW(resolved))
            {
                int sz = ::GetSystemMetrics(SM_CXSMICON);
                HICON h = LoadPngAsIcon(resolved, sz);
                if (h) {
                    owns = true;
                    store->Release();
                    return h;
                }
            }
        }
        PropVariantClear(&var);
        store->Release();
    }

    // 4) Extract from EXE
    DWORD pid = 0; ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid)
    {
        HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc)
        {
            wchar_t exe[MAX_PATH]{}; DWORD len = ARRAYSIZE(exe);
            if (::QueryFullProcessImageNameW(hProc, 0, exe, &len))
            {
                HICON hSmall = nullptr;
                UINT  extracted = ::ExtractIconExW(exe, 0, nullptr, &hSmall, 1);
                if (extracted > 0 && hSmall && hSmall != (HICON)1)
                {
                    owns = true;
                    ::CloseHandle(hProc);
                    return hSmall;
                }
            }
            ::CloseHandle(hProc);
        }
    }

    // 5) Stock Icon
    SHSTOCKICONINFO sii{ sizeof(sii) };
    if (SUCCEEDED(::SHGetStockIconInfo(
        SIID_APPLICATION, SHGSI_ICON | SHGSI_SMALLICON, &sii)))
    {
        owns = true;
        return sii.hIcon;
    }

    // 6) Fallback to built-in resource icon
    return (HICON)::LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCE(IDI_TRAY_ICON),
        IMAGE_ICON,
        ::GetSystemMetrics(SM_CXSMICON),
        ::GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR);
}

std::wstring TrayManager::GetWindowTitle(HWND hwnd)
{
    wchar_t buf[256]{}; ::GetWindowTextW(hwnd, buf, (int)std::size(buf));
    return *buf ? buf : I18N::S("untitled_window");
}

void TrayManager::ShowTrayMenu(HWND hwnd, POINT pt)
{
    HMENU m = ::CreatePopupMenu();
    if (!m) return;

    ::AppendMenuW(m, MF_STRING, IDM_RESTORE_ALL, I18N::S("menu_restore_all"));
    ::AppendMenuW(m, MF_STRING, IDM_SETTINGS, I18N::S("menu_settings"));
    ::AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(m, MF_STRING, IDM_ABOUT, I18N::S("menu_about"));
    ::AppendMenuW(m, MF_STRING, IDM_EXIT, I18N::S("menu_exit"));

    ::SetForegroundWindow(hwnd);
    ::TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    ::DestroyMenu(m);
}