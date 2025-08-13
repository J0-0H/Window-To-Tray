#pragma once
#ifndef VIRTUALDESKTOPMANAGER_H
#define VIRTUALDESKTOPMANAGER_H

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_set>

class VirtualDesktopManager {
public:
    VirtualDesktopManager();
    ~VirtualDesktopManager();

    bool Initialize();
    bool IsAvailable() const { return dllLoaded; }

    // Core functionality
    bool HideWindowToVirtualDesktop(HWND hwnd);
    bool RestoreWindowFromVirtualDesktop(HWND hwnd, int originalDesktop);

    // Virtual desktop management tools
    int  GetCurrentDesktopNumber();
    int  GetWindowDesktopNumber(HWND hwnd);
    bool MoveWindowToDesktopNumber(HWND hwnd, int desktopNumber);
    bool GoToDesktopNumber(int desktopNumber);
    int  CreateDesktop();                                  // Win11 only
    bool RemoveDesktop(int desktopNumber, int fallback);   // Win11 only

    // Hidden desktop info
    int  GetHiddenDesktopNumber() const { return hiddenDesktopNumber; }

    // UWP hidden window tracking
    void MarkUwpWindowHidden(HWND hwnd);
    void MarkUwpWindowRestored(HWND hwnd);
    int  GetUwpWindowCount() const;
    bool TryRemoveHiddenDesktopIfEmpty();

    // Force removal of the hidden desktop on exit
    void ForceRemoveHiddenDesktop();

private:
    // DLL loading state
    HMODULE dllHandle;
    bool    dllLoaded;

    // Hidden desktop state
    int     hiddenDesktopNumber;   // -1 means not yet determined
    GUID    hiddenDesktopGuid;     // For reuse across processes/crashes
    bool    createdHiddenDesktop;  // Was it created by this instance?
    std::string hiddenDesktopName; // Identifier name (visible on Win11)

    // UWP window tracking
    std::unordered_set<HWND> uwpHiddenWindows;
    int              uwpWindowCount;
    CRITICAL_SECTION countLock;

    // Typedefs for DLL function pointers
    typedef int  (*GetCurrentDesktopNumberFunc)();
    typedef int  (*GetDesktopCountFunc)();
    typedef GUID(*GetDesktopIdByNumberFunc)(int);
    typedef int  (*GetDesktopNumberByIdFunc)(GUID);
    typedef GUID(*GetWindowDesktopIdFunc)(HWND);
    typedef int  (*GetWindowDesktopNumberFunc)(HWND);
    typedef int  (*IsWindowOnCurrentVirtualDesktopFunc)(HWND);
    typedef int  (*MoveWindowToDesktopNumberFunc)(HWND, int);
    typedef int  (*GoToDesktopNumberFunc)(int);
    typedef int  (*SetDesktopNameFunc)(int, const char*);
    typedef int  (*GetDesktopNameFunc)(int, unsigned char*, size_t);
    typedef int  (*IsWindowOnDesktopNumberFunc)(HWND, int);
    typedef int  (*CreateDesktopFunc)();
    typedef int  (*RemoveDesktopFunc)(int, int);

    // Bound functions
    GetCurrentDesktopNumberFunc         pGetCurrentDesktopNumber;
    GetDesktopCountFunc                 pGetDesktopCount;
    GetDesktopIdByNumberFunc            pGetDesktopIdByNumber;
    GetDesktopNumberByIdFunc            pGetDesktopNumberById;
    GetWindowDesktopIdFunc              pGetWindowDesktopId;
    GetWindowDesktopNumberFunc          pGetWindowDesktopNumber;
    IsWindowOnCurrentVirtualDesktopFunc pIsWindowOnCurrentVirtualDesktop;
    MoveWindowToDesktopNumberFunc       pMoveWindowToDesktopNumber;
    GoToDesktopNumberFunc               pGoToDesktopNumber;
    SetDesktopNameFunc                  pSetDesktopName;
    GetDesktopNameFunc                  pGetDesktopName;
    IsWindowOnDesktopNumberFunc         pIsWindowOnDesktopNumber;
    CreateDesktopFunc                   pCreateDesktop;
    RemoveDesktopFunc                   pRemoveDesktop;

    // Utility functions
    bool LoadDllFunctions();
    bool EnsureHiddenDesktop();
    bool IsDesktopNumberValid(int n);
    bool RemoveHiddenDesktopInternal();

    // GUID persistence
    std::wstring GetProgramDirectoryW();
    std::wstring GetGuidFilePathW();
    bool SaveHiddenDesktopGuidAppend(const GUID& guid);
    bool LoadAllHiddenDesktopGuids(std::vector<GUID>& list);
    std::string GuidToString(const GUID& guid);
    GUID        StringToGuid(const std::string& s);
    static bool GuidEqual(const GUID& a, const GUID& b);

    // Find existing desktops
    int  FindExistingHiddenDesktopByGuid(const GUID& guid);
    int  FindExistingHiddenDesktopByGuidList(const std::vector<GUID>& list);
    int  FindExistingHiddenDesktopByName(const std::string& name);
    void TrySetHiddenDesktopName(int number, const std::string& name);
};

#endif // VIRTUALDESKTOPMANAGER_H