#include "UwpIconUtils.h"
#include <appmodel.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <propkey.h>
#include <vector>
#include <set>
#include <strsafe.h>
#include <string>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

using std::wstring;

static bool AumidFromProcess(DWORD pid, wstring& aumid)
{
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;

    UINT32 len = 0;
    LONG rc = ::GetApplicationUserModelId(h, &len, nullptr);
    if (rc != ERROR_INSUFFICIENT_BUFFER) {
        ::CloseHandle(h); return false;
    }
    std::vector<wchar_t> buf(len);
    rc = ::GetApplicationUserModelId(h, &len, buf.data());
    ::CloseHandle(h);
    if (rc == ERROR_SUCCESS && len > 1) {
        aumid.assign(buf.data(), len - 1);
        return true;
    }
    return false;
}

static bool AumidFromWindow(HWND hwnd, wstring& aumid)
{
    IPropertyStore* store = nullptr;
    if (FAILED(::SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store))))
        return false;

    PROPVARIANT var; PropVariantInit(&var);
    bool ok = false;
    if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &var)) &&
        var.vt == VT_LPWSTR && var.pwszVal && *var.pwszVal)
    {
        aumid = var.pwszVal;
        ok = true;
    }
    PropVariantClear(&var);
    store->Release();
    return ok;
}

namespace {
    struct EnumCtx {
        std::set<DWORD> visited;
        wstring aumid;
    };
    BOOL CALLBACK EnumChild(HWND h, LPARAM lp)
    {
        auto ctx = reinterpret_cast<EnumCtx*>(lp);
        if (!ctx || !ctx->aumid.empty()) return FALSE;

        DWORD pid = 0;
        ::GetWindowThreadProcessId(h, &pid);
        if (!pid || ctx->visited.count(pid)) return TRUE;
        ctx->visited.insert(pid);

        if (AumidFromProcess(pid, ctx->aumid) ||
            AumidFromWindow(h, ctx->aumid))
            return FALSE;

        ::EnumChildWindows(h, EnumChild, lp);
        return TRUE;
    }
}

static HICON IconFromAumid(const wstring& aumid, int sizePx, bool& owns)
{
    owns = false;
    wchar_t moniker[MAX_PATH * 2];
    swprintf_s(moniker, L"shell:AppsFolder\\%s", aumid.c_str());

    PIDLIST_ABSOLUTE pidl = nullptr;
    HRESULT hr = ::SHParseDisplayName(moniker, nullptr, &pidl, 0, nullptr);
    if (FAILED(hr)) {
        return nullptr;
    }

    IShellItemImageFactory* factory = nullptr;
    hr = ::SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&factory));
    ::CoTaskMemFree(pidl);
    if (FAILED(hr)) return nullptr;

    SIZE sz{ sizePx, sizePx };
    HBITMAP hbm = nullptr;
    hr = factory->GetImage(
        sz, SIIGBF_BIGGERSIZEOK | SIIGBF_ICONONLY | SIIGBF_CROPTOSQUARE, &hbm);
    factory->Release();

    if (FAILED(hr) || !hbm) return nullptr;

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = hbm;
    ii.hbmMask = ::CreateBitmap(sizePx, sizePx, 1, 1, nullptr);
    HICON hIcon = ::CreateIconIndirect(&ii);
    ::DeleteObject(ii.hbmMask);
    ::DeleteObject(hbm);

    if (hIcon) owns = true;
    return hIcon;
}

bool UwpIconUtils::GetUwpWindowIcon(HWND hwnd, int sizePx,
    HICON& outIcon, bool& owns)
{
    outIcon = nullptr; owns = false;
    if (!::IsWindow(hwnd)) return false;

    HRESULT hrInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool    needUninit = SUCCEEDED(hrInit);

    // Step 1: Try to get AUMID directly from the window
    wstring aumid;
    if (!AumidFromWindow(hwnd, aumid)) {
        // Step 2: If that fails, try getting it from the process or its children
        DWORD pid = 0; ::GetWindowThreadProcessId(hwnd, &pid);
        if (!AumidFromProcess(pid, aumid)) {
            EnumCtx ctx;
            ::EnumChildWindows(hwnd, EnumChild,
                reinterpret_cast<LPARAM>(&ctx));
            aumid = ctx.aumid;
        }
    }

    if (!aumid.empty())
        outIcon = IconFromAumid(aumid, sizePx, owns);

    if (needUninit)
        ::CoUninitialize();

    return outIcon != nullptr;
}