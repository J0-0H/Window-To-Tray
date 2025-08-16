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

    // ���ڶ�λģʽ
    enum class PositioningMode {
        NearTray,     // ͣ��������/��긽��
        CenterScreen  // ��������Ļ
    };

    /**
     * @brief ��ʾ�򼤻�ۺ����ɴ��ڡ�
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

    // ��ֹ�����͸�ֵ
    CollectionWindow(const CollectionWindow&) = delete;
    CollectionWindow& operator=(const CollectionWindow&) = delete;

    bool CreateAndShow();
    void Destroy();

    // ���ڹ��̺���Ϣ����
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // UI �������������
    void BuildUI();
    void PopulateList();
    void PositionWindow();

    // �¼�����
    void OnNotify(LPARAM lParam);
    void OnRestoreItem(int itemIndex);
    void OnRestoreAll();
    void OnKillFocus();
    void OnToggleCollectionMode(); // --- NEW ---

private:
    static CollectionWindow* s_instance; // ����ʵ������Ϊһ��ֻ��ʾһ��

    HWND              hParent_;
    HWND              hWnd_;
    TrayManager& trayManager_;
    PositioningMode   positioningMode_;
    DisableModeCallback disableModeCallback_; // --- NEW ---

    // �ؼ�
    HWND              hListView_;
    HWND              hBtnRestoreAll_;
    HWND              hBtnToggleMode_; // --- NEW ---
    HIMAGELIST        hImageList_; // ListView ��ͼ���б�

    UINT dpi_ = 96;
};

#endif // COLLECTION_WINDOW_H