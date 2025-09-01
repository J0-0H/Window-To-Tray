#pragma once
#ifndef COLLECTION_WINDOW_H
#define COLLECTION_WINDOW_H

#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <functional> // --- NEW ---
#include "TrayManager.h"

#pragma comment(lib, "Comctl32.lib")

class CollectionWindow {
public:
    // --- NEW ---
    // Callback to notify the main app to disable collection mode.
    using DisableModeCallback = std::function<void()>;


    enum class PositioningMode {
        NearTray,     
        CenterScreen  
    };

    /**
     * @brief 珆尨麼慾魂擄磁彶馨敦諳﹝
     * @param parent The parent window handle.
     * @param trayManager A reference to the TrayManager instance.
     * @param mode The desired positioning mode for the window.
     * @param onDisable Callback function to execute when the user clicks "disable mode".
     * @return true if the window was shown successfully, false otherwise.
     */
     // --- MODIFIED ---
    static bool Show(HWND parent, TrayManager& trayManager, PositioningMode mode, DisableModeCallback onDisable);

private:
    // --- MODIFIED ---
    CollectionWindow(HWND parent, TrayManager& trayManager, PositioningMode mode, DisableModeCallback onDisable);
    ~CollectionWindow();

    // 輦砦蕭探睿董硉
    CollectionWindow(const CollectionWindow&) = delete;
    CollectionWindow& operator=(const CollectionWindow&) = delete;

    bool CreateAndShow();
    void Destroy();

    // 敦諳徹最睿秏洘揭燴
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // UI 凳膘迵杅擂沓喃
    void BuildUI();
    void PopulateList();
    void PositionWindow();

    // --- NEW: 自适应高度&布局 ---
    void AdjustWindowToContent();  // 根据项目数动态调整高度（应用多长、应用少短）
    void UpdateLayout();           // 在窗口大小变化后更新子控件布局

    // 岈璃揭燴
    void OnNotify(LPARAM lParam);
    void OnRestoreItem(int itemIndex);
    void OnRestoreAll();
    void OnKillFocus();
    void OnToggleCollectionMode(); // --- NEW ---

    // --- NEW: 更聪明的“外点关闭 + 恢复不打断” ---
    static void CALLBACK WinEventProc(HWINEVENTHOOK hHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);
    void OnForegroundChanged(HWND newForeground);
    bool ShouldDismissOnDeactivate(HWND newForeground) const;

private:
    static CollectionWindow* s_instance; 

    HWND              hParent_;
    HWND              hWnd_;
    TrayManager& trayManager_;
    PositioningMode   positioningMode_;
    DisableModeCallback disableModeCallback_; // --- NEW ---

    // 諷璃
    HWND              hListView_;
    HWND              hBtnRestoreAll_;
    HWND              hBtnToggleMode_; // --- NEW ---
    HIMAGELIST        hImageList_; // ListView 

    UINT dpi_ = 96;

    // --- NEW: WinEvent hook & 自动关闭相关状态 ---
    HWINEVENTHOOK     hEventHook_ = nullptr;
    HWND              lastRestoredHwnd_ = nullptr;
    ULONGLONG         suppressCloseUntil_ = 0; // GetTickCount64() 时间戳
    bool              closing_ = false;

    // --- NEW: UI metrics cache ---
    int margin_ = 12;
    int btnH_ = 36;
    int btnRestoreW_ = 120;
    int btnToggleW_ = 160;
    int iconSize_ = 0;
};

#endif // COLLECTION_WINDOW_H