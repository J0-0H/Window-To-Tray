#pragma once
#ifndef UWP_ICON_UTILS_H
#define UWP_ICON_UTILS_H

#include <windows.h>

namespace UwpIconUtils
{
    // �� hwnd ���� UWP/MSIX/PWA �ȴ��Ӧ�ã��򷵻� true��
    // �����ָ���ߴ� (sizePx) �� HICON��
    // owns==true ʱ�����÷����� DestroyIcon��
    bool GetUwpWindowIcon(HWND hwnd, int sizePx, HICON& outIcon, bool& owns);
}

#endif // UWP_ICON_UTILS_H