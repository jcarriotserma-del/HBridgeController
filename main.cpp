#include <windows.h>
#include <commctrl.h>
#include "MainWindow.h"
#pragma comment(lib, "comctl32.lib")

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);
    MainWindow mainWindow(hInstance);
    if (!mainWindow.Create()) { MessageBoxW(nullptr, L"Échec création fenêtre", L"Erreur", MB_ICONERROR); return 1; }
    MSG msg; while (GetMessage(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return static_cast<int>(msg.wParam);
}
