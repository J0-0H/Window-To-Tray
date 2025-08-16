#pragma once
#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_MAIN_ICON       101
#define IDI_TRAY_ICON       102
#define IDM_EXIT            1001
#define IDM_RESTORE_ALL     1002
#define IDM_ABOUT           1003
#define IDM_SETTINGS        1004

#define WM_TRAY_CALLBACK    (WM_USER + 1)
#define WM_RESTORE_WINDOW   (WM_USER + 2)

// Settings dialog controls (pure-code dialog, still need IDs)
#define IDC_CHK_USEVD                   2001
#define IDC_COMBO_LANG                  2002
#define IDC_HOTKEY_MIN_TOP              2003
#define IDC_HOTKEY_HIDE_ALL             2004
#define IDC_BTN_SAVE                    2005
#define IDC_BTN_CANCEL                  2006
#define IDC_LABEL_LANG                  2007
#define IDC_LABEL_USEVD                 2008
#define IDC_LABEL_HK_MIN                2009
#define IDC_LABEL_HK_HIDE_ALL           2010
#define IDC_LABEL_HK_TIP                2011
#define IDC_CHK_USE_COLLECTION_MODE     2012
#define IDC_HOTKEY_SHOW_COLLECTION      2013
#define IDC_LABEL_HK_SHOW_COLLECTION    2014

// Collection Window controls
#define IDC_LIST_WINDOWS                3001
#define IDC_BTN_RESTORE_ALL_COLLECTION  3002
// --- NEW ---
#define IDC_BTN_TOGGLE_COLLECTION_MODE  3003


// Hotkey IDs
#define ID_HOTKEY_MIN_TOP       0x2001
#define ID_HOTKEY_HIDE_ALL      0x2002
#define ID_HOTKEY_SHOW_COLLECTION 0x2003

#endif
