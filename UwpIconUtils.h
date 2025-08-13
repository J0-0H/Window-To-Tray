#pragma once
#ifndef UWP_ICON_UTILS_H
#define UWP_ICON_UTILS_H

#include <windows.h>

namespace UwpIconUtils
{
    // 若 hwnd 属于 UWP/MSIX/PWA 等打包应用，则返回 true，
    // 并输出指定尺寸 (sizePx) 的 HICON。
    // owns==true 时，调用方负责 DestroyIcon。
    bool GetUwpWindowIcon(HWND hwnd, int sizePx, HICON& outIcon, bool& owns);
}

#endif // UWP_ICON_UTILS_H