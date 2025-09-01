#include "WindowManager.h"
#include "VirtualDesktopManager.h"
#include "UwpIconUtils.h"
#include <strsafe.h>
#include <psapi.h>
#include <shlwapi.h>
#include <appmodel.h>
#include <shlobj.h>
#include <propsys.h>
#include <propkey.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")

VirtualDesktopManager* WindowManager::virtualDesktopManager = nullptr;
ITaskbarList3* WindowManager::taskbarList = nullptr;
bool WindowManager::useVirtualDesktop = true; // Enabled by default

bool WindowManager::Initialize()
{
    // The main application thread has already been initialized with COINIT_APARTMENTTHREADED.
    // We can create the TaskbarList instance here.
    HRESULT hr = ::CoCreateInstance(CLSID_TaskbarList, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbarList));

    if (SUCCEEDED(hr) && taskbarList)
    {
        taskbarList->HrInit();
        return true;
    }

    taskbarList = nullptr;
    return false;
}

void WindowManager::Cleanup()
{
    if (taskbarList)
    {
        taskbarList->Release();
        taskbarList = nullptr;
    }
}

void WindowManager::SetVirtualDesktopManager(VirtualDesktopManager* vdm)
{
    virtualDesktopManager = vdm;
}

void WindowManager::SetUseVirtualDesktop(bool enable)
{
    useVirtualDesktop = enable;
}

bool WindowManager::GetUseVirtualDesktop()
{
    return useVirtualDesktop;
}

bool WindowManager::EnsureTaskbar()
{
    // The instance is now created during Initialize, so we just check for its existence.
    return taskbarList != nullptr;
}

bool WindowManager::IsApplicationFrameHostWindow(HWND hwnd)
{
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return false;

    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    wchar_t processName[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    bool isFrameHost = false;

    if (::QueryFullProcessImageNameW(hProcess, 0, processName, &size))
    {
        wchar_t* fileName = ::PathFindFileNameW(processName);
        isFrameHost = (::_wcsicmp(fileName, L"ApplicationFrameHost.exe") == 0);
    }

    ::CloseHandle(hProcess);
    return isFrameHost;
}

bool WindowManager::HasValidAUMID(HWND hwnd)
{
    // Try to get AUMID from window properties
    IPropertyStore* store = nullptr;
    if (FAILED(::SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store))))
        return false;

    PROPVARIANT var;
    PropVariantInit(&var);
    bool hasAumid = false;

    if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &var)) &&
        var.vt == VT_LPWSTR && var.pwszVal && *var.pwszVal)
    {
        hasAumid = true;
    }

    PropVariantClear(&var);
    store->Release();

    if (hasAumid) return true;

    // Try to get AUMID from the process
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return false;

    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    UINT32 length = 0;
    LONG result = ::GetApplicationUserModelId(hProcess, &length, nullptr);
    ::CloseHandle(hProcess);

    return (result == ERROR_SUCCESS || result == ERROR_INSUFFICIENT_BUFFER);
}

bool WindowManager::IsUWPApplication(HWND hwnd)
{
    // Method 1: Try to get UWP icon (most accurate)
    HICON testIcon = nullptr;
    bool owns = false;
    if (UwpIconUtils::GetUwpWindowIcon(hwnd, 32, testIcon, owns))
    {
        if (testIcon && owns)
            ::DestroyIcon(testIcon);
        return true;
    }

    // Method 2: Check if it's an ApplicationFrameHost process
    if (IsApplicationFrameHostWindow(hwnd))
    {
        return true;
    }

    // Method 3: Check for a valid AUMID
    if (HasValidAUMID(hwnd))
    {
        return true;
    }

    // Method 4: Traditional package check (fallback)
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return false;

    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    UINT32 length = 0;
    LONG result = ::GetPackageFullName(hProcess, &length, nullptr);
    ::CloseHandle(hProcess);

    if (result == ERROR_SUCCESS || result == ERROR_INSUFFICIENT_BUFFER)
    {
        return true;
    }

    return false;
}

bool WindowManager::HideWindowTraditional(HWND hwnd)
{
    if (!::IsWindow(hwnd)) return false;

    // 1) Remove the taskbar button
    if (EnsureTaskbar())
        taskbarList->DeleteTab(hwnd);

    // 2) Modify extended styles to hide from Alt-Tab
    LONG_PTR exBefore = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    LONG_PTR exAfter = (exBefore | WS_EX_TOOLWINDOW) & ~WS_EX_APPWINDOW;
    ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exAfter);

    // Force a non-client area refresh to apply style changes immediately
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // 3) Hide the window
    ::ShowWindow(hwnd, SW_HIDE);
    return true;
}

bool WindowManager::ShowWindowTraditional(HWND hwnd)
{
    if (!::IsWindow(hwnd)) return false;

    // 1) Restore extended styles
    LONG_PTR exBefore = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    LONG_PTR exAfter = (exBefore | WS_EX_APPWINDOW) & ~WS_EX_TOOLWINDOW;
    ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exAfter);

    // Force a non-client area refresh
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // 2) Restore the taskbar button
    if (EnsureTaskbar())
        taskbarList->AddTab(hwnd);

    // 3) Show the window
    ::ShowWindow(hwnd, SW_RESTORE);
    ::SetForegroundWindow(hwnd);
    return true;
}

bool WindowManager::HideWindowVirtual(HWND hwnd)
{
    if (!virtualDesktopManager || !virtualDesktopManager->IsAvailable())
    {
        return false;
    }
    return virtualDesktopManager->HideWindowToVirtualDesktop(hwnd);
}

bool WindowManager::ShowWindowVirtual(HWND hwnd, int originalDesktop)
{
    if (!virtualDesktopManager || !virtualDesktopManager->IsAvailable())
    {
        return false;
    }

    bool result = virtualDesktopManager->RestoreWindowFromVirtualDesktop(hwnd, originalDesktop);
    if (result)
    {
        ::ShowWindow(hwnd, SW_RESTORE);
        ::SetForegroundWindow(hwnd);
    }
    return result;
}

bool WindowManager::HideWindowFromTaskbar(HWND hwnd)
{
    if (!::IsWindow(hwnd))
        return false;

    const bool isUWP = IsUWPApplication(hwnd);

    // First, use the traditional Win32 method for all windows
    bool okTraditional = HideWindowTraditional(hwnd);

    // For UWP apps, apply the virtual desktop enhancement to hide taskbar remnants
    if (isUWP && useVirtualDesktop)
    {
        bool okVirtual = HideWindowVirtual(hwnd);

        if (okVirtual && virtualDesktopManager)
        {
            virtualDesktopManager->MarkUwpWindowHidden(hwnd);
        }

        return okTraditional || okVirtual;
    }

    return okTraditional;
}

bool WindowManager::ShowWindowInTaskbar(HWND hwnd, int originalDesktop)
{
    if (!::IsWindow(hwnd))
        return false;

    const bool isUWP = IsUWPApplication(hwnd);

    // For UWP apps, first try to move it back from the hidden virtual desktop
    if (isUWP && useVirtualDesktop)
    {
        (void)ShowWindowVirtual(hwnd, originalDesktop);

        // Just untrack, don't try to remove the hidden desktop here
        // (to avoid popping up all UWP windows at once).
        if (virtualDesktopManager)
        {
            virtualDesktopManager->MarkUwpWindowRestored(hwnd);
        }
    }

    // Finally, use the traditional method to restore it to the taskbar
    return ShowWindowTraditional(hwnd);
}

bool WindowManager::IsValidTargetWindow(HWND hwnd)
{
    if (!::IsWindow(hwnd) || !::IsWindowVisible(hwnd))
        return false;

    // Only process top-level windows
    if (::GetParent(hwnd))
        return false;

    // Exclude system windows like the taskbar itself
    wchar_t cls[64]{};
    ::GetClassNameW(hwnd, cls, 64);
    if (wcscmp(cls, L"Shell_TrayWnd") == 0 ||
        wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0)
        return false;

    // Exclude tool windows
    LONG_PTR ex = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW)
        return false;

    // Exclude windows that are too small
    RECT rc{};
    ::GetWindowRect(hwnd, &rc);
    return (rc.right - rc.left >= 100) && (rc.bottom - rc.top >= 50);
}

bool WindowManager::MinimizeToTray(HWND hwnd)
{
    if (!IsValidTargetWindow(hwnd))
        return false;
    return HideWindowFromTaskbar(hwnd);
}

void WindowManager::TryRemoveHiddenDesktopIfUnused()
{
    if (virtualDesktopManager && useVirtualDesktop)
    {
        virtualDesktopManager->TryRemoveHiddenDesktopIfEmpty();
    }
}