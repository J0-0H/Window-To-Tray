[English](#english) | [中文](#中文)

<a name="english"></a>

# Window-To-Tray

A powerful and flexible utility for Windows that allows you to minimize any application window to the system tray instead of the taskbar. It offers robust support for both classic Win32 applications and modern UWP (Microsoft Store) apps.

<!-- Placeholder for a GIF/video demonstrating the core functionality -->
> **Demonstration**
>
> 
![example](https://github.com/user-attachments/assets/e5a8786c-9a26-42ea-9ae4-1ebc15001785)





## ✨ Features & Advantages

*   **Minimize Any Window**: Works with almost any window, including traditional Win32 programs and modern UWP/MSIX apps.
*   **Multiple Ways to Minimize**:
    *   **Right-click** a window's minimize button.
    *   Use a global **hotkey** to minimize the current foreground window (Default: `Ctrl + Alt + M`).
    *   Use a global **hotkey** to hide all visible windows on the current desktop (Default: `Ctrl + Alt + X`).
*   **Seamless UWP Support**: Uses an optional virtual desktop technique to perfectly hide UWP apps (like Calculator, Settings, etc.) without leaving ghost windows on the taskbar.
*   **High-Quality Icons**: Intelligently extracts the best possible icon for each application, including high-resolution icons for UWP apps. It also processes icons with solid backgrounds to make them transparent and clear in the system tray.
*   **Easy Restoration**: Simply **left-click** an icon in the tray to restore the corresponding window to its original state.
*   **Full Control**: Right-click the main program icon in the tray for a menu to restore all windows, open settings, or exit the application.
*   **Safe Exit**: Automatically restores all minimized windows before the application quits.
*   **Configurable**: Easily change hotkeys, UI language, and other behaviors through a simple settings dialog.
*   **Lightweight & Portable**: No installation required. All settings are saved in an `.ini` file in the same directory.

## ⚙️ How It Works

Window-To-Tray uses smart techniques to hide different types of windows:
*   For **classic Win32 apps**, it uses standard Windows methods to hide the window and remove it from the taskbar.
*   For modern **UWP apps**, it uses a special virtual desktop method by default. This is the key to preventing non-functional "ghost windows" from appearing on the taskbar, ensuring a clean and seamless experience.
*   It uses a global mouse hook to detect when you **right-click a window's minimize button**, which triggers the hide-to-tray action.

## 🚀 How to Use

1.  Download the latest release from the Releases page.
2.  Unzip the archive and run `WindowToTray.exe`. No installation is necessary.
3.  The main program icon will appear in your system tray.

**To Minimize a Window:**
*   **Method 1**: Right-click the minimize button (`—`) of the window you want to hide.
*   **Method 2**: Click on the window to make it active, then press the "Minimize Top Window" hotkey (default `Ctrl + Alt + M`).

**To Restore a Window:**
*   **Left-click** its icon in the system tray.

## 🔧 Configuration

You can customize the application's behavior by opening the settings dialog.

1.  **Right-click** the main Window-To-Tray icon in the system tray.
2.  Select **"Settings"** from the context menu.

<!-- Placeholder for a screenshot of the settings dialog -->
> **Settings Dialog**
>
> <img width="1074" height="645" alt="image" src="https://github.com/user-attachments/assets/d996830d-afcc-480a-affd-d25f50e10890" />


Here you can configure:
*   **Use virtual desktop enhancement for UWP**: This option enables the special handling for UWP apps. It is highly recommended to keep this enabled.
*   **Language**: Switch the UI language between English and Chinese.
*   **Hotkeys**: Set custom key combinations for minimizing the top window and for hiding all windows.

### How to Disable the Virtual Desktop Feature
While highly recommended for the best experience, you can disable this feature if you wish.
*   In the Settings dialog, **uncheck** the box that says "Use virtual desktop enhancement for UWP".
*   Click **"Save"**.

**Warning**: Disabling this may cause UWP apps (like Calculator, Photos, etc.) to leave a non-functional 'ghost' window on your taskbar when you try to minimize them to the tray.

## ⚠️ Important Notes

*   **Administrator Privileges**: If you want to manage applications that are running as an administrator, you may need to run Window-To-Tray with administrator privileges as well.
*   **Antivirus Software**: Global mouse hooks can sometimes be flagged by antivirus software as suspicious (a false positive). If you encounter issues, please add an exception for the application.
*   **Hotkey Conflicts**: If a hotkey doesn't work, it might already be registered by another application or the system. Try a different key combination.
*   **Virtual Desktop Feature**: This feature is enabled by default and is crucial for correctly hiding UWP apps. If you choose to disable it in the settings, be aware that UWP apps may not hide properly.
*   **Virtual Desktop Feature**: If you want to achieve a similar hiding effect as Win32 applications, you should do so through: Settings -> System -> Multitasking -> Desktops. Change both options to "On the desktop I'm using only".(without it may cause UWP apps (like Calculator, Photos, etc.) to leave a non-functional 'ghost' window on your taskbar when you try to minimize them to the tray.)

## 🙏 Acknowledgements

*   **Inspiration**: This project was heavily inspired by **RBTray**, a classic tool with similar goals. Window-To-Tray aims to modernize the concept with better support for newer Windows versions and UWP applications.
*   **Virtual Desktop Access**: The core virtual desktop functionality is made possible by the **VirtualDesktopAccessor.dll** library, which provides a clean interface to Windows' undocumented virtual desktop APIs. A big thank you to its creators and maintainers.

---
<br>
<a name="中文"></a>

# Window-To-Tray (窗口到托盘)

一个功能强大且灵活的 Windows 小工具，可以将任何应用程序窗口最小化到系统托盘区，而不是任务栏。它为经典的 Win32 应用和现代的 UWP (应用商店) 应用提供了稳定可靠的支持。

<!-- 演示视频/GIF占位符 -->
> **功能演示**
>
> 
![Uploading example.gif…]()




## ✨ 功能与优点

*   **最小化任意窗口**: 适用于几乎所有窗口，包括传统的 Win32 程序和现代的 UWP/MSIX 应用。
*   **多种最小化方式**:
    *   **右键点击** 窗口的最小化按钮。
    *   使用全局 **快捷键** 最小化当前最前方的窗口 (默认: `Ctrl + Alt + M`)。
    *   使用全局 **快捷键** 隐藏当前桌面上的所有可见窗口 (默认: `Ctrl + Alt + X`)。
*   **无缝的 UWP 支持**: 使用可选的虚拟桌面技术，完美隐藏 UWP 应用（如计算器、设置等），不会在任务栏上留下“幽灵窗口”。
*   **高质量图标**: 智能地为每个应用程序提取最佳图标，包括 UWP 应用的高清图标。它还会对带有纯色背景的图标进行处理，使其在托盘中背景透明，观感更佳。
*   **轻松恢复**: 只需 **左键单击** 托盘区的图标，即可将对应的窗口恢复到原始状态。
*   **完全控制**: 右键单击托盘区的主程序图标，可以打开菜单来恢复所有窗口、进入设置或退出程序。
*   **安全退出**: 在程序退出前，会自动恢复所有已最小化的窗口。
*   **可配置**: 通过简洁的设置对话框，轻松更改快捷键、界面语言和其他行为。
*   **轻量便携**: 无需安装，所有设置都保存在同目录下的 `.ini` 文件中。

## ⚙️ 工作原理

本程序使用智能技术来隐藏不同类型的窗口：
*   对于 **经典 Win32 应用**，它使用标准的 Windows 方法来隐藏窗口并将其从任务栏移除。
*   对于现代的 **UWP 应用**，它默认使用一种特殊的虚拟桌面方法。这是防止任务栏上出现无法交互的“幽灵窗口”的关键，能确保体验干净无缝。
*   它使用全局鼠标钩子来检测您何时 **右键单击窗口的最小化按钮**，并触发隐藏到托盘的操作。

## 🚀 如何使用

1.  从 Releases 页面下载最新的发行版。
2.  解压缩文件并运行 `WindowToTray.exe`。无需安装。
3.  程序的主图标会出现在您的系统托盘区。

**最小化一个窗口:**
*   **方法一**: 在您想隐藏的窗口的最小化按钮 (`—`) 上单击鼠标右键。
*   **方法二**: 单击窗口使其成为活动窗口，然后按下“最小化顶部窗口”的快捷键 (默认为 `Ctrl + Alt + M`)。

**恢复一个窗口:**
*   在系统托盘区 **左键单击** 它的图标。

## 🔧 配置

您可以通过设置对话框来自定义程序的行为。

1.  在系统托盘区 **右键单击** Window-To-Tray 的主图标。
2.  从菜单中选择 **“设置”**。

<!-- 设置对话框截图占位符 -->
> **设置对话框**
>
> <img width="1066" height="590" alt="image" src="https://github.com/user-attachments/assets/3acfdf37-e331-47aa-8168-27106777bcca" />


在这里您可以配置：
*   **使用虚拟桌面增强 UWP 隐藏**: 此选项为 UWP 应用启用特殊处理。强烈建议保持开启。
*   **语言**: 在中文和英文之间切换界面语言。
*   **快捷键**: 为最小化顶部窗口和隐藏所有窗口设置自定义的按键组合。

### 如何禁用虚拟桌面功能
虽然我们强烈建议开启此功能以获得最佳体验，但您也可以选择禁用它。
*   在设置对话框中，**取消勾选** “使用虚拟桌面增强 UWP 隐藏” 复选框。
*   点击 **“保存”**。

**警告**：禁用此功能可能会导致 UWP 应用（如计算器、照片等）在您尝试将其最小化到托盘时，在任务栏上留下一个无法交互的“幽灵”窗口。

## ⚠️ 注意事项

*   **管理员权限**: 如果您希望管理以管理员身份运行的程序，您可能也需要以管理员权限运行本程序。
*   **杀毒软件**: 全局鼠标钩子有时可能会被杀毒软件标记为可疑行为（误报）。如果遇到问题，请为本程序添加例外。
*   **快捷键冲突**: 如果快捷键不生效，它可能已被其他程序或系统占用。请尝试更换一个按键组合。
*   **虚拟桌面功能**: 此功能默认开启，对于正确隐藏 UWP 应用至关重要。如果您在设置中选择禁用它，请注意 UWP 应用可能无法被正确隐藏。
*   **虚拟桌面功能**: 如果想要实现和win32应用一样的隐藏功能，应该通过 设置-> 系统->多任务处理->桌面（将两个选项都调为仅限我正在使用的桌面）(如果不使用它的话，uwp应用的图标将任残留在任务栏)

## 🙏 致谢

*   **灵感来源**: 本项目的灵感主要来源于经典的 **RBTray** 工具。Window-To-Tray 旨在将这一概念现代化，为新版 Windows 和 UWP 应用提供更好的支持。
*   **虚拟桌面访问**: 核心的虚拟桌面功能得以实现，离不开 **VirtualDesktopAccessor.dll** 库。它为 Windows 未公开的虚拟桌面 API 提供了一个清晰的接口。非常感谢它的创建者和维护者。
