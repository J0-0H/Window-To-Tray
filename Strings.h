#pragma once
#ifndef STRINGS_H
#define STRINGS_H

#include <string>

namespace I18N {

    enum class Language {
        Chinese = 1,
        English = 0
    };

    void SetLanguage(Language lang);
    Language GetLanguage();

    // ��ȡ���ػ��ַ�����key Ϊ�̶�Լ����խ�ַ�����������
    const wchar_t* S(const char* key);

    // һЩ���� Key��
    // "menu_restore_all", "menu_about", "menu_exit", "menu_settings"
    // "about_title", "about_text", "tray_tooltip"
    // "error_com_init", "error_hotkey_reg_failed", "already_running", "already_running_title"
    // "untitled_window"
    // "settings_title", "settings_use_virtual_desktop", "settings_language"
    // "settings_hotkey_min_top", "settings_hotkey_hide_all", "settings_btn_save", "settings_btn_cancel", "settings_hotkey_tip"

} // namespace I18N

#endif