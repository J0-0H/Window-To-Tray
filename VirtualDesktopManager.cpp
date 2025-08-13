#include "VirtualDesktopManager.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdio>

VirtualDesktopManager::VirtualDesktopManager()
    : dllHandle(nullptr),
    dllLoaded(false),
    hiddenDesktopNumber(-1),
    createdHiddenDesktop(false),
    hiddenDesktopName("WindowToTray-Hidden"),
    uwpWindowCount(0),
    pGetCurrentDesktopNumber(nullptr),
    pGetDesktopCount(nullptr),
    pGetDesktopIdByNumber(nullptr),
    pGetDesktopNumberById(nullptr),
    pGetWindowDesktopId(nullptr),
    pGetWindowDesktopNumber(nullptr),
    pIsWindowOnCurrentVirtualDesktop(nullptr),
    pMoveWindowToDesktopNumber(nullptr),
    pGoToDesktopNumber(nullptr),
    pSetDesktopName(nullptr),
    pGetDesktopName(nullptr),
    pIsWindowOnDesktopNumber(nullptr),
    pCreateDesktop(nullptr),
    pRemoveDesktop(nullptr)
{
    ::InitializeCriticalSection(&countLock);
    ::ZeroMemory(&hiddenDesktopGuid, sizeof(hiddenDesktopGuid));
}

VirtualDesktopManager::~VirtualDesktopManager()
{
    // Best-effort attempt to remove the hidden desktop on exit
    ForceRemoveHiddenDesktop();

    ::DeleteCriticalSection(&countLock);

    if (dllHandle) {
        ::FreeLibrary(dllHandle);
        dllHandle = nullptr;
    }
}

bool VirtualDesktopManager::Initialize()
{
    dllHandle = ::LoadLibraryW(L"VirtualDesktopAccessor.dll");
    if (!dllHandle) {
        return false;
    }
    if (!LoadDllFunctions()) {
        ::FreeLibrary(dllHandle);
        dllHandle = nullptr;
        return false;
    }

    dllLoaded = true;

    // First, try to find a leftover hidden desktop from a previous run via its GUID
    std::vector<GUID> savedGuids;
    if (LoadAllHiddenDesktopGuids(savedGuids) && !savedGuids.empty()) {
        int byGuidList = FindExistingHiddenDesktopByGuidList(savedGuids);
        if (byGuidList >= 0) {
            hiddenDesktopNumber = byGuidList;
            if (pGetDesktopIdByNumber && IsDesktopNumberValid(byGuidList)) {
                hiddenDesktopGuid = pGetDesktopIdByNumber(byGuidList);
            }
            createdHiddenDesktop = false; // Reusing
            return true;
        }
    }

    // Second, try to find it by its name (Win11+)
    int byName = FindExistingHiddenDesktopByName(hiddenDesktopName);
    if (byName >= 0) {
        hiddenDesktopNumber = byName;
        createdHiddenDesktop = false;
        if (pGetDesktopIdByNumber && IsDesktopNumberValid(byName)) {
            hiddenDesktopGuid = pGetDesktopIdByNumber(byName);
        }
    }

    return true;
}

bool VirtualDesktopManager::LoadDllFunctions()
{
#define BIND(name) reinterpret_cast<name##Func>(::GetProcAddress(dllHandle, #name))

    pGetCurrentDesktopNumber = BIND(GetCurrentDesktopNumber);
    pGetDesktopCount = BIND(GetDesktopCount);
    pGetDesktopIdByNumber = BIND(GetDesktopIdByNumber);
    pGetDesktopNumberById = BIND(GetDesktopNumberById);
    pGetWindowDesktopId = BIND(GetWindowDesktopId);
    pGetWindowDesktopNumber = BIND(GetWindowDesktopNumber);
    pIsWindowOnCurrentVirtualDesktop = BIND(IsWindowOnCurrentVirtualDesktop);
    pMoveWindowToDesktopNumber = BIND(MoveWindowToDesktopNumber);
    pGoToDesktopNumber = BIND(GoToDesktopNumber);
    pSetDesktopName = BIND(SetDesktopName);
    pGetDesktopName = BIND(GetDesktopName);
    pIsWindowOnDesktopNumber = BIND(IsWindowOnDesktopNumber);
    pCreateDesktop = BIND(CreateDesktop);
    pRemoveDesktop = BIND(RemoveDesktop);

#undef BIND

    // Check for essential functions
    return pGetCurrentDesktopNumber &&
        pGetDesktopCount &&
        pGetWindowDesktopNumber &&
        pMoveWindowToDesktopNumber &&
        pGoToDesktopNumber;
}

bool VirtualDesktopManager::IsDesktopNumberValid(int n)
{
    if (!pGetDesktopCount || n < 0) return false;
    int count = pGetDesktopCount();
    return (n >= 0 && n < count);
}

std::string VirtualDesktopManager::GuidToString(const GUID& g)
{
    char buf[64];
    sprintf_s(buf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        g.Data1, g.Data2, g.Data3,
        g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
        g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return buf;
}

GUID VirtualDesktopManager::StringToGuid(const std::string& s)
{
    GUID g{};
    if (s.size() >= 38) {
        sscanf_s(s.c_str(), "{%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
            &g.Data1, &g.Data2, &g.Data3,
            &g.Data4[0], &g.Data4[1], &g.Data4[2], &g.Data4[3],
            &g.Data4[4], &g.Data4[5], &g.Data4[6], &g.Data4[7]);
    }
    return g;
}

bool VirtualDesktopManager::GuidEqual(const GUID& a, const GUID& b)
{
    return a.Data1 == b.Data1 && a.Data2 == b.Data2 && a.Data3 == b.Data3 &&
        memcmp(a.Data4, b.Data4, sizeof(a.Data4)) == 0;
}

std::wstring VirtualDesktopManager::GetProgramDirectoryW()
{
    wchar_t path[MAX_PATH] = { 0 };
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return full.substr(0, pos);
    }
    return L".";
}

std::wstring VirtualDesktopManager::GetGuidFilePathW()
{
    return GetProgramDirectoryW() + L"\\hidden_desktops.guids";
}

bool VirtualDesktopManager::SaveHiddenDesktopGuidAppend(const GUID& g)
{
    std::wstring filePath = GetGuidFilePathW();
    std::ofstream file(filePath, std::ios::out | std::ios::app);
    if (!file.is_open()) {
        return false;
    }
    file << GuidToString(g) << "\n";
    file.close();
    return true;
}

bool VirtualDesktopManager::LoadAllHiddenDesktopGuids(std::vector<GUID>& list)
{
    list.clear();
    std::wstring filePath = GetGuidFilePathW();
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && isspace(line.back())) {
            line.pop_back();
        }
        if (line.empty()) continue;

        GUID g = StringToGuid(line);
        GUID zero{};
        if (!GuidEqual(g, zero)) list.push_back(g);
    }
    file.close();

    return !list.empty();
}

int VirtualDesktopManager::FindExistingHiddenDesktopByGuid(const GUID& guid)
{
    if (!pGetDesktopNumberById) return -1;
    int n = pGetDesktopNumberById(guid);
    if (IsDesktopNumberValid(n)) return n;
    return -1;
}

int VirtualDesktopManager::FindExistingHiddenDesktopByGuidList(const std::vector<GUID>& list)
{
    if (!pGetDesktopCount) return -1;
    int count = pGetDesktopCount();
    if (count <= 0) return -1;

    if (pGetDesktopIdByNumber) {
        for (int i = 0; i < count; ++i) {
            GUID cur = pGetDesktopIdByNumber(i);
            for (const auto& g : list) {
                if (GuidEqual(cur, g)) return i;
            }
        }
        return -1;
    }

    if (pGetDesktopNumberById) {
        for (const auto& g : list) {
            int n = pGetDesktopNumberById(g);
            if (IsDesktopNumberValid(n)) return n;
        }
    }
    return -1;
}

int VirtualDesktopManager::FindExistingHiddenDesktopByName(const std::string& name)
{
    if (!pGetDesktopCount || !pGetDesktopName) return -1;
    int count = pGetDesktopCount();
    if (count <= 0) return -1;

    unsigned char utf8[256]{};
    for (int i = 0; i < count; ++i) {
        if (pGetDesktopName(i, utf8, sizeof(utf8)) == 0 && utf8[0] != 0) {
            if (name.compare(reinterpret_cast<char*>(utf8)) == 0) return i;
        }
    }
    return -1;
}

void VirtualDesktopManager::TrySetHiddenDesktopName(int number, const std::string& name)
{
    if (pSetDesktopName && IsDesktopNumberValid(number)) {
        (void)pSetDesktopName(number, name.c_str());
    }
}

bool VirtualDesktopManager::EnsureHiddenDesktop()
{
    // If already valid, do nothing.
    if (IsDesktopNumberValid(hiddenDesktopNumber)) return true;

    // Reset state
    hiddenDesktopNumber = -1;
    ::ZeroMemory(&hiddenDesktopGuid, sizeof(hiddenDesktopGuid));

    // 1) Try to reuse a desktop from the saved GUID list
    std::vector<GUID> savedGuids;
    if (LoadAllHiddenDesktopGuids(savedGuids) && !savedGuids.empty()) {
        int n = FindExistingHiddenDesktopByGuidList(savedGuids);
        if (n >= 0) {
            hiddenDesktopNumber = n;
            if (pGetDesktopIdByNumber && IsDesktopNumberValid(n)) {
                hiddenDesktopGuid = pGetDesktopIdByNumber(n);
            }
            createdHiddenDesktop = false;
            return true;
        }
    }

    // 2) Try to reuse a desktop by its name (Win11+)
    int m = FindExistingHiddenDesktopByName(hiddenDesktopName);
    if (m >= 0) {
        hiddenDesktopNumber = m;
        createdHiddenDesktop = false;
        if (pGetDesktopIdByNumber && IsDesktopNumberValid(m)) {
            hiddenDesktopGuid = pGetDesktopIdByNumber(m);
        }
        return true;
    }

    // 3) Try to create a new desktop (Win11+)
    if (pCreateDesktop) {
        int n = pCreateDesktop();
        if (n >= 0) {
            hiddenDesktopNumber = n;
            createdHiddenDesktop = true;
            if (pGetDesktopIdByNumber && IsDesktopNumberValid(n)) {
                hiddenDesktopGuid = pGetDesktopIdByNumber(n);
                SaveHiddenDesktopGuidAppend(hiddenDesktopGuid);
            }
            TrySetHiddenDesktopName(hiddenDesktopNumber, hiddenDesktopName);
            return true;
        }
    }

    // 4) For Win10, reuse the last existing desktop if at least 2 exist
    if (pGetDesktopCount) {
        int count = pGetDesktopCount();
        if (count >= 2) {
            hiddenDesktopNumber = count - 1;
            createdHiddenDesktop = false;
            if (pGetDesktopIdByNumber && IsDesktopNumberValid(hiddenDesktopNumber)) {
                hiddenDesktopGuid = pGetDesktopIdByNumber(hiddenDesktopNumber);
            }
            TrySetHiddenDesktopName(hiddenDesktopNumber, hiddenDesktopName);
            return true;
        }
    }

    return false;
}

bool VirtualDesktopManager::HideWindowToVirtualDesktop(HWND hwnd)
{
    if (!dllLoaded || !::IsWindow(hwnd)) return false;
    if (!EnsureHiddenDesktop()) return false;

    int rc = pMoveWindowToDesktopNumber ? pMoveWindowToDesktopNumber(hwnd, hiddenDesktopNumber) : -1;
    return (rc == 0);
}

bool VirtualDesktopManager::RestoreWindowFromVirtualDesktop(HWND hwnd, int originalDesktop)
{
    if (!dllLoaded || !::IsWindow(hwnd)) return false;

    // 1) Determine the target desktop
    int target = originalDesktop;
    if (!IsDesktopNumberValid(target)) {
        int current = GetCurrentDesktopNumber();
        target = IsDesktopNumberValid(current) ? current : 0;
    }

    // 2) Move the window to the target
    int rc = pMoveWindowToDesktopNumber ? pMoveWindowToDesktopNumber(hwnd, target) : -1;
    if (rc == 0) {
        if (pGoToDesktopNumber) (void)pGoToDesktopNumber(target);
        return true;
    }

    // 3) Fallback to desktop 0 if the original target failed
    if (target != 0 && IsDesktopNumberValid(0) && pMoveWindowToDesktopNumber &&
        pMoveWindowToDesktopNumber(hwnd, 0) == 0) {
        if (pGoToDesktopNumber) (void)pGoToDesktopNumber(0);
        return true;
    }

    return false;
}

bool VirtualDesktopManager::TryRemoveHiddenDesktopIfEmpty()
{
    ::EnterCriticalSection(&countLock);
    bool canRemove = uwpHiddenWindows.empty();
    ::LeaveCriticalSection(&countLock);

    if (!canRemove) return false;
    return RemoveHiddenDesktopInternal();
}

void VirtualDesktopManager::ForceRemoveHiddenDesktop()
{
    ::EnterCriticalSection(&countLock);
    uwpHiddenWindows.clear();
    uwpWindowCount = 0;
    ::LeaveCriticalSection(&countLock);

    (void)RemoveHiddenDesktopInternal();
}

bool VirtualDesktopManager::RemoveHiddenDesktopInternal()
{
    if (!dllLoaded || !IsDesktopNumberValid(hiddenDesktopNumber)) return false;

    // Can only physically remove on Win11+
    if (!pRemoveDesktop) {
        return false;
    }

    int current = GetCurrentDesktopNumber();
    int fallback = (current == hiddenDesktopNumber) ? 0 : current;
    if (!IsDesktopNumberValid(fallback)) fallback = 0;

    int rc = pRemoveDesktop(hiddenDesktopNumber, fallback);
    if (rc == 0) {
        // Reset state after successful removal
        hiddenDesktopNumber = -1;
        createdHiddenDesktop = false;
        ::ZeroMemory(&hiddenDesktopGuid, sizeof(hiddenDesktopGuid));
        return true;
    }
    return false;
}

void VirtualDesktopManager::MarkUwpWindowHidden(HWND hwnd)
{
    if (!hwnd) return;
    ::EnterCriticalSection(&countLock);
    if (uwpHiddenWindows.insert(hwnd).second) {
        ++uwpWindowCount;
    }
    ::LeaveCriticalSection(&countLock);
}

void VirtualDesktopManager::MarkUwpWindowRestored(HWND hwnd)
{
    if (!hwnd) return;
    ::EnterCriticalSection(&countLock);
    if (uwpHiddenWindows.erase(hwnd) > 0) {
        if (uwpWindowCount > 0) --uwpWindowCount;
    }
    ::LeaveCriticalSection(&countLock);
}

int VirtualDesktopManager::GetUwpWindowCount() const
{
    ::EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&countLock));
    int n = uwpWindowCount;
    ::LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&countLock));
    return n;
}

int VirtualDesktopManager::GetCurrentDesktopNumber()
{
    return (dllLoaded && pGetCurrentDesktopNumber) ? pGetCurrentDesktopNumber() : -1;
}

int VirtualDesktopManager::GetWindowDesktopNumber(HWND hwnd)
{
    return (dllLoaded && pGetWindowDesktopNumber) ? pGetWindowDesktopNumber(hwnd) : -1;
}

bool VirtualDesktopManager::MoveWindowToDesktopNumber(HWND hwnd, int desktopNumber)
{
    return (dllLoaded && pMoveWindowToDesktopNumber) ?
        (pMoveWindowToDesktopNumber(hwnd, desktopNumber) == 0) : false;
}

bool VirtualDesktopManager::GoToDesktopNumber(int desktopNumber)
{
    return (dllLoaded && pGoToDesktopNumber) ?
        (pGoToDesktopNumber(desktopNumber) == 0) : false;
}

int VirtualDesktopManager::CreateDesktop()
{
    return (dllLoaded && pCreateDesktop) ? pCreateDesktop() : -1;
}

bool VirtualDesktopManager::RemoveDesktop(int desktopNumber, int fallbackDesktop)
{
    return (dllLoaded && pRemoveDesktop) ?
        (pRemoveDesktop(desktopNumber, fallbackDesktop) == 0) : false;
}