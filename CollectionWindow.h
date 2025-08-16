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

    // 窗口定位模式
    enum class PositioningMode {
        NearTray,     // 停靠在托盘/光标附近
        CenterScreen  // 居中于屏幕
    };

    /**
     * @brief 显示或激活聚合收纳窗口。
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

    // 禁止拷贝和赋值
    CollectionWindow(const CollectionWindow&) = delete;
    CollectionWindow& operator=(const CollectionWindow&) = delete;

    bool CreateAndShow();
    void Destroy();

    // 窗口过程和消息处理
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // UI 构建与数据填充
    void BuildUI();
    void PopulateList();
    void PositionWindow();

    // 事件处理
    void OnNotify(LPARAM lParam);
    void OnRestoreItem(int itemIndex);
    void OnRestoreAll();
    void OnKillFocus();
    void OnToggleCollectionMode(); // --- NEW ---

private:
    static CollectionWindow* s_instance; // 单例实例，因为一次只显示一个

    HWND              hParent_;
    HWND              hWnd_;
    TrayManager& trayManager_;
    PositioningMode   positioningMode_;
    DisableModeCallback disableModeCallback_; // --- NEW ---

    // 控件
    HWND              hListView_;
    HWND              hBtnRestoreAll_;
    HWND              hBtnToggleMode_; // --- NEW ---
    HIMAGELIST        hImageList_; // ListView 的图标列表

    UINT dpi_ = 96;
};

#endif // COLLECTION_WINDOW_H