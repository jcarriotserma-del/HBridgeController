#pragma once
#include <windows.h>
#include "SerialController.h"
#include "Protocol.h"

class MainWindow {
public:
    MainWindow(HINSTANCE hInst);
    ~MainWindow();
    HWND Create(HWND parent = nullptr);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // Actions
    void OnConnect();
    void OnEStop();
    void OnReset();
    void OnApplyFreq();
    void OnApplyDeadTime();
    void OnDutyChanged(int value);
    void OnSerialData(const std::string& line);
    void OnConnectionChanged(bool connected);

    // Helpers
    void LogMessage(const wchar_t* msg);
    void UpdateStatus(const wchar_t* text);
    void EnableControls(bool enable);

private:
    // Interface
    HINSTANCE m_hInst;
    HWND m_hwnd = nullptr;
    HWND m_hComboPorts = nullptr, m_hBtnConnect = nullptr, m_hEditFreq = nullptr, m_hSliderDuty = nullptr;
    HWND m_hLabelDuty = nullptr, m_hEditDead = nullptr, m_hBtnEStop = nullptr, m_hBtnReset = nullptr;
    HWND m_hLogBox = nullptr, m_hStatusBar = nullptr;
    HWND m_hBtnFreqApply = nullptr;
    HWND m_hBtnDeadApply = nullptr;

    // ✅ Trace COM Brute (conservé)
    HWND m_hRawLogBox = nullptr;
    HWND m_hChkRawTrace = nullptr;
    bool m_rawTraceEnabled = false;
    void LogRawTx(const std::string& data);
    void LogRawRx(const std::string& data);

    // État courant (conservé pour la logique, même sans graphique)
    uint32_t m_currentFreq = 120000; // 120kHz par défaut
    uint8_t m_currentDuty = 50;
    float m_currentDeadTime = 0.3f;  // 300ns par défaut

    // ❌ Graphique supprimé : DrawGraph et UpdateGraph retirés

    SerialController m_serial;
    void CreateControls();
    void RefreshPortList();
    static std::string WideStringToString(const wchar_t* wstr);
};