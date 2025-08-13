#include "Strings.h"
#include <cstring>

namespace I18N {

    static Language g_lang = Language::Chinese;

    void SetLanguage(Language lang) {
        g_lang = lang;
    }
    Language GetLanguage() { return g_lang; }

    static const wchar_t* zh(const char* key) {
        if (std::strcmp(key, "menu_restore_all") == 0) return L"�ָ����д���";
        if (std::strcmp(key, "menu_about") == 0)       return L"����";
        if (std::strcmp(key, "menu_exit") == 0)        return L"�˳�";
        if (std::strcmp(key, "menu_settings") == 0)    return L"���á�";

        if (std::strcmp(key, "about_title") == 0)      return L"����";
        if (std::strcmp(key, "about_text") == 0)       return L"Window-To-Tray\n\n����������ͼ�꼴�ɻָ���Ӧ���ڣ�\n�Ҽ��������ʾ�˵���\n�����˳�ʱ���Զ��ָ����д��ڲ������������档";
        if (std::strcmp(key, "tray_tooltip") == 0)     return L"Window-To-Tray";

        if (std::strcmp(key, "error_com_init") == 0)           return L"COM ���ʼ��ʧ�ܣ�";
        if (std::strcmp(key, "error_hotkey_reg_failed") == 0)  return L"ע���ݼ�ʧ�ܣ�������ϵͳ�����������ͻ��";
        if (std::strcmp(key, "already_running") == 0)          return L"Ӧ�ó����������У�";
        if (std::strcmp(key, "already_running_title") == 0)    return L"Window-To-Tray";

        if (std::strcmp(key, "untitled_window") == 0)          return L"(�ޱ��ⴰ��)";

        if (std::strcmp(key, "settings_title") == 0)           return L"����";
        if (std::strcmp(key, "settings_use_virtual_desktop") == 0) return L"ʹ������������ǿ UWP ����";
        if (std::strcmp(key, "settings_language") == 0)        return L"����";
        if (std::strcmp(key, "settings_hotkey_min_top") == 0)  return L"��ݼ�����С����������";
        if (std::strcmp(key, "settings_hotkey_hide_all") == 0) return L"��ݼ����������д���";
        if (std::strcmp(key, "settings_btn_save") == 0)        return L"����";
        if (std::strcmp(key, "settings_btn_cancel") == 0)      return L"ȡ��";
        if (std::strcmp(key, "settings_hotkey_tip") == 0)      return L"��ʾ������ʹ�� Ctrl/Shift/Alt ��ϣ���ע��ʧ���������ϵͳ���������ͻ��";
        return L"";
    }

    static const wchar_t* en(const char* key) {
        if (std::strcmp(key, "menu_restore_all") == 0) return L"Restore All Windows";
        if (std::strcmp(key, "menu_about") == 0)       return L"About";
        if (std::strcmp(key, "menu_exit") == 0)        return L"Exit";
        if (std::strcmp(key, "menu_settings") == 0)    return L"Settings��";

        if (std::strcmp(key, "about_title") == 0)      return L"About";
        if (std::strcmp(key, "about_text") == 0)       return L"Window-To-Tray\n\nLeft click a tray icon to restore the window.\nRight click to open the menu.\nAll windows will be restored and hidden desktops cleaned up on exit.";
        if (std::strcmp(key, "tray_tooltip") == 0)     return L"Window-To-Tray";

        if (std::strcmp(key, "error_com_init") == 0)           return L"Failed to initialize COM library!";
        if (std::strcmp(key, "error_hotkey_reg_failed") == 0)  return L"Registering hotkey failed: it may conflict with the system or another app.";
        if (std::strcmp(key, "already_running") == 0)          return L"The application is already running!";
        if (std::strcmp(key, "already_running_title") == 0)    return L"Window-To-Tray";

        if (std::strcmp(key, "untitled_window") == 0)          return L"(Untitled Window)";

        if (std::strcmp(key, "settings_title") == 0)           return L"Settings";
        if (std::strcmp(key, "settings_use_virtual_desktop") == 0) return L"Use virtual desktop enhancement for UWP";
        if (std::strcmp(key, "settings_language") == 0)        return L"Language";
        if (std::strcmp(key, "settings_hotkey_min_top") == 0)  return L"Hotkey: Minimize top window";
        if (std::strcmp(key, "settings_hotkey_hide_all") == 0) return L"Hotkey: Hide all windows";
        if (std::strcmp(key, "settings_btn_save") == 0)        return L"Save";
        if (std::strcmp(key, "settings_btn_cancel") == 0)      return L"Cancel";
        if (std::strcmp(key, "settings_hotkey_tip") == 0)      return L"Tip: Use Ctrl/Shift/Alt combinations; if registration fails it may be due to conflicts.";
        return L"";
    }

    const wchar_t* S(const char* key) {
        return (g_lang == Language::Chinese) ? zh(key) : en(key);
    }

} // namespace I18N