#pragma once
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <windows.h>
#include <commctrl.h>
#include <functional>
#include "Settings.h"
#include "Resource.h"
#include "Strings.h"

#pragma comment(lib, "Comctl32.lib")

class SettingsDialog {
public:
    /**
     * @brief Shows the modal settings dialog.
     * @param parent The parent window handle.
     * @param current The current settings to display.
     * @param outSettings Receives the new settings if the user clicks "Save".
     * @return true if the user saved the settings, false otherwise.
     */
    static bool ShowModal(HWND parent, const Settings& current, Settings& outSettings);

private:
    SettingsDialog(HWND parent, const Settings& current);
    ~SettingsDialog();

    bool Create();
    void Destroy();
    INT_PTR DoModalLoop(Settings& outSettings);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void BuildUi();
    void ApplyLocalization();
    void CenterToParent();

    void OnSave();
    void OnCancel();

    // HOTKEY control helpers
    static WORD MakeHotkeyWord(const Hotkey& hk);
    static Hotkey FromHotkeyWord(WORD w);

private:
    HWND hParent_;
    HWND hWnd_;
    Settings cur_;

    // Controls
    HWND hChkUseVD_;
    HWND hComboLang_;
    HWND hHkMinTop_;
    HWND hHkHideAll_;
    HWND hBtnSave_;
    HWND hBtnCancel_;
    // --- NEW ---
    HWND hChkUseCollection_;
    HWND hHkShowCollection_;

    bool saved_ = false;
    Settings result_;

    // Cached DPI, default is 96
    UINT dpi_ = 96;
};

#endif