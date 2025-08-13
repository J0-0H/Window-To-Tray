#include "Strings.h"
#include <cstring>

namespace I18N {

    static Language g_lang = Language::Chinese;

    void SetLanguage(Language lang) {
        g_lang = lang;
    }
    Language GetLanguage() { return g_lang; }

    static const wchar_t* zh(const char* key) {
        if (std::strcmp(key, "menu_restore_all") == 0) return L"恢复所有窗口";
        if (std::strcmp(key, "menu_about") == 0)       return L"关于";
        if (std::strcmp(key, "menu_exit") == 0)        return L"退出";
        if (std::strcmp(key, "menu_settings") == 0)    return L"设置…";

        if (std::strcmp(key, "about_title") == 0)      return L"关于";
        if (std::strcmp(key, "about_text") == 0)       return L"Window-To-Tray\n\n左键点击托盘图标即可恢复对应窗口，\n右键点击可显示菜单，\n程序退出时会自动恢复所有窗口并清理虚拟桌面。";
        if (std::strcmp(key, "tray_tooltip") == 0)     return L"Window-To-Tray";

        if (std::strcmp(key, "error_com_init") == 0)           return L"COM 库初始化失败！";
        if (std::strcmp(key, "error_hotkey_reg_failed") == 0)  return L"注册快捷键失败：可能与系统或其它程序冲突。";
        if (std::strcmp(key, "already_running") == 0)          return L"应用程序已在运行！";
        if (std::strcmp(key, "already_running_title") == 0)    return L"Window-To-Tray";

        if (std::strcmp(key, "untitled_window") == 0)          return L"(无标题窗口)";

        if (std::strcmp(key, "settings_title") == 0)           return L"设置";
        if (std::strcmp(key, "settings_use_virtual_desktop") == 0) return L"使用虚拟桌面增强 UWP 隐藏";
        if (std::strcmp(key, "settings_language") == 0)        return L"语言";
        if (std::strcmp(key, "settings_hotkey_min_top") == 0)  return L"快捷键：最小化顶部窗口";
        if (std::strcmp(key, "settings_hotkey_hide_all") == 0) return L"快捷键：隐藏所有窗口";
        if (std::strcmp(key, "settings_btn_save") == 0)        return L"保存";
        if (std::strcmp(key, "settings_btn_cancel") == 0)      return L"取消";
        if (std::strcmp(key, "settings_hotkey_tip") == 0)      return L"提示：建议使用 Ctrl/Shift/Alt 组合；如注册失败则可能与系统或他程序冲突。";
        return L"";
    }

    static const wchar_t* en(const char* key) {
        if (std::strcmp(key, "menu_restore_all") == 0) return L"Restore All Windows";
        if (std::strcmp(key, "menu_about") == 0)       return L"About";
        if (std::strcmp(key, "menu_exit") == 0)        return L"Exit";
        if (std::strcmp(key, "menu_settings") == 0)    return L"Settings…";

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