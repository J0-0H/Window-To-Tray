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

    int getMinDistanceToEdge(int iconWidth, int iconHeight) const {
        if (isEmpty) return 0;
        return std::min({ left, top, iconWidth - 1 - right, iconHeight - 1 - bottom });
    }
};

// --- OPTIMIZED: Rewritten to use GDI+ for high-quality scaling ---
HICON RemoveIconBackground_FloodFillAndScale(HICON hIcon, bool& outOwnsNewIcon)
{
    outOwnsNewIcon = false;
    if (!hIcon || !EnsureGdiPlus()) return nullptr;

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

    std::vector<DWORD> pixels(bm.bmWidth * bm.bmHeight);
    {
        Bitmap srcBitmap(info.hbmColor, nullptr);
        if (srcBitmap.GetLastStatus() != Ok) {
            cleanup();
            return nullptr;
        }
        Rect rect(0, 0, bm.bmWidth, bm.bmHeight);
        BitmapData data;
        if (srcBitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) {
            cleanup();
            return nullptr;
        }
        memcpy(pixels.data(), data.Scan0, pixels.size() * sizeof(DWORD));
        srcBitmap.UnlockBits(&data);
    }

    DWORD bgColor = pixels[0];
    if ((bgColor >> 24) != 0) { // If not transparent, flood-fill from corners
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
        AddSeed(0, 0); AddSeed(bm.bmWidth - 1, 0);
        AddSeed(0, bm.bmHeight - 1); AddSeed(bm.bmWidth - 1, bm.bmHeight - 1);
        while (!q.empty()) {
            POINT p = q.front(); q.pop();
            AddSeed(p.x + 1, p.y); AddSeed(p.x - 1, p.y);
            AddSeed(p.x, p.y + 1); AddSeed(p.x, p.y - 1);
        }
    }

    ContentBounds bounds;
    for (int y = 0; y < bm.bmHeight; y++) {
        for (int x = 0; x < bm.bmWidth; x++) {
            if ((pixels[y * bm.bmWidth + x] >> 24) > 16) {
                bounds.addPoint(x, y);
            }
        }
    }

    if (bounds.isEmpty) {
        cleanup();
        outOwnsNewIcon = true;
        return CopyIcon(hIcon); // Return a copy as we own it
    }

    int contentWidth = bounds.width();
    int contentHeight = bounds.height();
    int minDistance = bounds.getMinDistanceToEdge(bm.bmWidth, bm.bmHeight);

    double scale = std::min((double)bm.bmWidth / contentWidth, (double)bm.bmHeight / contentHeight);
    scale = std::min(scale, 3.0); // Limit max scale
    scale = std::max(scale, 1.0); // Don't shrink
    if (minDistance < 4) {
        scale = std::min(scale, 1.2); // Limit scale if content is already near edge
    }

    HICON hNewIcon = nullptr;
    if (scale > 1.01) {
        Bitmap finalBitmap(bm.bmWidth, bm.bmHeight, PixelFormat32bppARGB);
        Graphics g(&finalBitmap);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

        // Create a temporary bitmap containing only the icon content
        Bitmap contentBitmap(contentWidth, contentHeight, bm.bmWidth * 4, PixelFormat32bppARGB,
            (BYTE*)pixels.data() + (bounds.top * bm.bmWidth + bounds.left) * 4);

        int newContentWidth = (int)(contentWidth * scale);
        int newContentHeight = (int)(contentHeight * scale);
        int offsetX = (bm.bmWidth - newContentWidth) / 2;
        int offsetY = (bm.bmHeight - newContentHeight) / 2;

        g.DrawImage(&contentBitmap, offsetX, offsetY, newContentWidth, newContentHeight);
        finalBitmap.GetHICON(&hNewIcon);
    }

    if (!hNewIcon) {
        // If no scaling was performed or failed, create icon from (potentially modified) pixel buffer
        Bitmap finalBitmap(bm.bmWidth, bm.bmHeight, bm.bmWidth * 4, PixelFormat32bppARGB, (BYTE*)pixels.data());
        finalBitmap.GetHICON(&hNewIcon);
    }

    cleanup();

    if (hNewIcon) {
        outOwnsNewIcon = true; // GDI+ GetHICON creates a new icon that we must manage
    }
    return hNewIcon;
}

TrayManager::TrayManager(HWND mainWindow, ShowCollectionCallback showCollectionCb)
    : mainWindow(mainWindow),
    nextIconId(1000),
    collectionModeActive_(false),
    showCollectionCallback_(showCollectionCb) {
}

TrayManager::~TrayManager() { RemoveAllTrayIcons(); }

void TrayManager::SetCollectionMode(bool isEnabled) {
    collectionModeActive_ = isEnabled;
}

bool TrayManager::AddWindowToTray(HWND hwnd, int originalDesktop, bool createIndividualIcon)
{
    if (!IsWindow(hwnd)) return false;
    if (IsWindowInTray(hwnd)) return false;

    TrayIcon ti{};
    ti.targetWindow = hwnd;
    ti.windowTitle = GetWindowTitle(hwnd);
    ti.originalIcon = GetWindowIcon(hwnd, ti.ownsIcon);
    ti.wasMaximized = ::IsZoomed(hwnd) ? true : false;
    ti.originalDesktop = originalDesktop;
    ti.isUwp = WindowManager::IsUWPApplication(hwnd);

    ti.nid.cbSize = sizeof(NOTIFYICONDATA);
    ti.nid.hWnd = mainWindow;
    ti.nid.uID = nextIconId;
    ti.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    ti.nid.uCallbackMessage = WM_TRAY_CALLBACK;
    ti.nid.hIcon = ti.originalIcon;
    ::wcscpy_s(ti.nid.szTip, ti.windowTitle.c_str());

    bool addedSuccessfully = true;
    if (createIndividualIcon) {
        addedSuccessfully = ::Shell_NotifyIconW(NIM_ADD, &ti.nid);
    }

    if (addedSuccessfully) {
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

    if (it->second.nid.uFlags != 0) {
        ::Shell_NotifyIconW(NIM_DELETE, &it->second.nid);
    }

    if (it->second.ownsIcon && it->second.originalIcon)
        ::DestroyIcon(it->second.originalIcon);

    trayIcons.erase(it);

    // Check if any UWP windows are left in the tray
    bool anyUwpLeft = false;
    for (const auto& kv : trayIcons) {
        if (kv.second.isUwp) { anyUwpLeft = true; break; }
    }
    if (!anyUwpLeft) {
        WindowManager::TryRemoveHiddenDesktopIfUnused();
    }

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
        if (p.second.nid.uFlags != 0) {
            ::Shell_NotifyIconW(NIM_DELETE, &p.second.nid);
        }
        if (p.second.ownsIcon && p.second.originalIcon)
            ::DestroyIcon(p.second.originalIcon);
    }
    trayIcons.clear();
}

bool TrayManager::HandleTrayMessage(WPARAM wParam, LPARAM lParam)
{
    UINT id = static_cast<UINT>(wParam);
    UINT msg = LOWORD(lParam);

    if (msg == WM_LBUTTONUP || msg == NIN_SELECT || msg == NIN_KEYSELECT) {
        if (collectionModeActive_) {
            if (showCollectionCallback_) {
                showCollectionCallback_();
            }
            return true;
        }
        else {
            if (id != 1) { // Ignore clicks on the main app icon
                return RestoreWindowFromTray(id);
            }
        }
    }
    else if (msg == WM_RBUTTONUP || msg == WM_CONTEXTMENU) {
        POINT pt;
        ::GetCursorPos(&pt);
        ShowContextMenu(pt);
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

const std::map<UINT, TrayIcon>& TrayManager::GetTrayIcons() const
{
    return trayIcons;
}

// --- OPTIMIZED: Fixed HICON resource leaks ---
HICON TrayManager::GetWindowIcon(HWND hwnd, bool& owns)
{
    owns = false;
    HICON originalIcon = nullptr;
    bool originalOwns = false;

    // --- Helper lambda to process and clean up icons ---
    auto processIcon = [&](HICON hIn, bool inOwns) -> HICON {
        bool processedOwns = false;
        HICON hProcessed = RemoveIconBackground_FloodFillAndScale(hIn, processedOwns);

        if (hProcessed) {
            if (inOwns) {
                ::DestroyIcon(hIn); // Destroy original if we owned it
            }
            owns = processedOwns; // We now own the new processed icon
            return hProcessed;
        }

        // Processing failed, return original
        owns = inOwns;
        return hIn;
        };

    // 1. UWP icon retrieval
    int sz = ::GetSystemMetrics(SM_CXICON);
    if (UwpIconUtils::GetUwpWindowIcon(hwnd, sz, originalIcon, originalOwns) && originalIcon)
    {
        return processIcon(originalIcon, originalOwns);
    }

    // 2. Traditional Win32 icon retrieval
    originalIcon = (HICON)::SendMessage(hwnd, WM_GETICON, ICON_SMALL2, 0);
    if (!originalIcon) originalIcon = (HICON)::SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!originalIcon) originalIcon = (HICON)::SendMessage(hwnd, WM_GETICON, ICON_BIG, 0);
    if (!originalIcon) originalIcon = (HICON)::GetClassLongPtr(hwnd, GCLP_HICONSM);
    if (!originalIcon) originalIcon = (HICON)::GetClassLongPtr(hwnd, GCLP_HICON);
    if (originalIcon)
    {
        // We don't own icons from SendMessage/GetClassLongPtr
        return processIcon(originalIcon, false);
    }

    // 3. Shell properties (modern apps)
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
                int iconSize = ::GetSystemMetrics(SM_CXSMICON);
                originalIcon = LoadPngAsIcon(resolved, iconSize);
                if (originalIcon) {
                    // LoadPngAsIcon creates an icon we must destroy.
                    originalOwns = true;
                }
            }
        }
        PropVariantClear(&var);
        store->Release();
        if (originalIcon) {
            return processIcon(originalIcon, originalOwns);
        }
    }

    // 4. Fallback: extract from the host executable
    DWORD pid = 0; ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid)
    {
        HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc)
        {
            wchar_t exe[MAX_PATH]{}; DWORD len = ARRAYSIZE(exe);
            if (::QueryFullProcessImageNameW(hProc, 0, exe, &len))
            {
                UINT extracted = ::ExtractIconExW(exe, 0, nullptr, &originalIcon, 1);
                if (extracted > 0 && originalIcon && originalIcon != (HICON)1)
                {
                    // ExtractIconExW gives us an icon we must destroy.
                    originalOwns = true;
                }
                else {
                    originalIcon = nullptr;
                }
            }
            ::CloseHandle(hProc);
            if (originalIcon) {
                return processIcon(originalIcon, originalOwns);
            }
        }
    }

    // 5. Absolute fallback: generic application icon
    SHSTOCKICONINFO sii{ sizeof(sii) };
    if (SUCCEEDED(::SHGetStockIconInfo(SIID_APPLICATION, SHGSI_ICON | SHGSI_SMALLICON, &sii)))
    {
        // sii.hIcon is a copy that we must destroy.
        return processIcon(sii.hIcon, true);
    }

    // 6. Last resort: load from our own resources (we don't own this one)
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

void TrayManager::ShowContextMenu(POINT pt)
{
    HMENU m = ::CreatePopupMenu();
    if (!m) return;

    const UINT ID_MENU_SEPARATOR = (UINT)-1;

    ::AppendMenuW(m, MF_OWNERDRAW | MF_STRING, IDM_RESTORE_ALL, I18N::S("menu_restore_all"));
    ::AppendMenuW(m, MF_OWNERDRAW | MF_STRING, IDM_SETTINGS, I18N::S("menu_settings"));
    ::AppendMenuW(m, MF_OWNERDRAW | MF_STRING, ID_MENU_SEPARATOR, nullptr);
    ::AppendMenuW(m, MF_OWNERDRAW | MF_STRING, IDM_ABOUT, I18N::S("menu_about"));
    ::AppendMenuW(m, MF_OWNERDRAW | MF_STRING, IDM_EXIT, I18N::S("menu_exit"));

    if (trayIcons.empty()) {
        ::EnableMenuItem(m, IDM_RESTORE_ALL, MF_BYCOMMAND | MF_GRAYED);
    }

    ::SetForegroundWindow(mainWindow);
    ::TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, mainWindow, nullptr);
    ::DestroyMenu(m);
}