/*
 * PROJECT:    imgui-framebuffer
 * FILE:       ImGuiWin32GdiExample.cpp
 * PURPOSE:    Implementation for Dear ImGui Framebuffer Win32 GDI Example
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Windows.h>

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    ::MessageBoxW(
        nullptr,
        L"Hello World!\n",
        L"ImGuiWin32GdiExample",
        0);

    return 0;
}
