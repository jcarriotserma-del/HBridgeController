#pragma once
#include <windows.h>
#include "SerialController.h"
#include "Protocol.h"

class MainWindow {
public:
    MainWindow(HINSTANCE hInst); ~MainWindow();
    HWND Create(HWND parent = nullptr);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void OnConnect(); void OnEStop(); void OnReset(); void OnApplyFreq(); void OnApplyDeadTime(); void OnDutyChanged(int value);
    void OnSerialData(const std::string& line); void OnConnectionChanged(bool connected);
    void LogMessage(const wchar_t* msg); void UpdateStatus(const wchar_t* text); void EnableControls(bool enable);
private:
    HINSTANCE m_hInst; HWND m_hwnd = nullptr;
    HWND m_hComboPorts = nullptr, m_hBtnConnect = nullptr, m_hEditFreq = nullptr, m_hSliderDuty = nullptr;
    HWND m_hLabelDuty = nullptr, m_hEditDead = nullptr, m_hBtnEStop = nullptr, m_hBtnReset = nullptr;
    HWND m_hLogBox = nullptr, m_hStatusBar = nullptr;
    HWND m_hBtnFreqApply = nullptr;
    HWND m_hBtnDeadApply = nullptr;
    HWND m_hGraph = nullptr;

    // État courant pour le dessin (enregistré quand on applique les paramètres)
    uint32_t m_currentFreq = 5000;
    uint8_t m_currentDuty = 50;
    float m_currentDeadTime = 1.5;

    void DrawGraph(HDC hdc);
    void UpdateGraph();
    SerialController m_serial;
    void CreateControls(); void RefreshPortList();
    static std::string WideStringToString(const wchar_t* wstr);
};
