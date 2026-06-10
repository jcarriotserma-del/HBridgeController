#include "MainWindow.h"
#include "resource.h"
#include <commctrl.h>
#include <string>
#pragma comment(lib, "comctl32.lib")

MainWindow::MainWindow(HINSTANCE hInst) : m_hInst(hInst) {
    // Callbacks standard
    m_serial.SetOnDataReceived([this](const std::string& line) {
        PostMessage(m_hwnd, WM_SERIAL_DATA, 0, reinterpret_cast<LPARAM>(new std::string(line)));
        });
    m_serial.SetOnConnectionChanged([this](bool connected) {
        PostMessage(m_hwnd, WM_CONNECTION_CHANGED, connected, 0);
        });

    // Callbacks Trace COM Brute
    m_serial.SetOnRawDataTx([this](const std::string& data) {
        PostMessage(m_hwnd, WM_USER + 10, 0, reinterpret_cast<LPARAM>(new std::string(data)));
        });
    m_serial.SetOnRawDataRx([this](const std::string& data) {
        PostMessage(m_hwnd, WM_USER + 11, 0, reinterpret_cast<LPARAM>(new std::string(data)));
        });
}

MainWindow::~MainWindow() {
    m_serial.Disconnect();
}

HWND MainWindow::Create(HWND parent) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInst;
    wc.lpszClassName = L"HBridgeWndClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    // ✅ Hauteur ajustée à 850px pour contenir la trace COM
    m_hwnd = CreateWindowExW(0, L"HBridgeWndClass", L"Contrôle Pont en H - STM32 (C++)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 850,
        parent, nullptr, m_hInst, this);

    if (m_hwnd) {
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(this));
        CreateControls();
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
    return m_hwnd;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (msg == WM_CREATE) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(reinterpret_cast<LPCREATESTRUCTW>(lp)->lpCreateParams));
        return 0;
    }
    if (!pThis) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_CONNECT: pThis->OnConnect(); break;
        case IDC_ESTOP_BTN: pThis->OnEStop(); break;
        case IDC_RESET_BTN: pThis->OnReset(); break;
        case IDC_FREQ_APPLY: pThis->OnApplyFreq(); break;
        case IDC_DEAD_APPLY: pThis->OnApplyDeadTime(); break;
        case ID_EXIT: DestroyWindow(hwnd); break;

            // ✅ Gestion de la case à cocher Trace COM (ID 5001)
        case 5001:
            if (HIWORD(wp) == BN_CLICKED) {
                pThis->m_rawTraceEnabled = !pThis->m_rawTraceEnabled;
                if (!pThis->m_rawTraceEnabled) SetWindowTextW(pThis->m_hRawLogBox, L"");
            }
            break;
        }
        return 0;

    case WM_HSCROLL:
        if ((HWND)lp == pThis->m_hSliderDuty) {
            if (LOWORD(wp) == TB_ENDTRACK || LOWORD(wp) == TB_THUMBPOSITION) {
                int val = static_cast<int>(SendMessage(pThis->m_hSliderDuty, TBM_GETPOS, 0, 0));
                pThis->OnDutyChanged(val);
            }
        }
        return 0;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
        if (dis->CtlID == IDC_GRAPH_AREA && dis->hwndItem == pThis->m_hGraph) {
            pThis->DrawGraph(dis->hDC);
            return TRUE;
        }
        break;
    }

                    // ✅ Messages Trace COM
    case WM_USER + 10: {
        auto* data = reinterpret_cast<std::string*>(lp);
        if (data) { pThis->LogRawTx(*data); delete data; }
        return 0;
    }
    case WM_USER + 11: {
        auto* data = reinterpret_cast<std::string*>(lp);
        if (data) { pThis->LogRawRx(*data); delete data; }
        return 0;
    }

    case WM_SERIAL_DATA: {
        auto* line = reinterpret_cast<std::string*>(lp);
        if (line) { pThis->OnSerialData(*line); delete line; }
        return 0;
    }
    case WM_CONNECTION_CHANGED:
        pThis->OnConnectionChanged(wp != 0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void MainWindow::CreateControls() {
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    auto addStatic = [hFont, this](HWND parent, const wchar_t* txt, int x, int y, int w, int height) {
        HWND h = CreateWindowW(L"STATIC", txt, WS_CHILD | WS_VISIBLE, x, y, w, height, parent, nullptr, m_hInst, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        };
    auto addBtn = [hFont, this](HWND parent, const wchar_t* txt, int x, int y, int w, int height, int id) {
        HWND h = CreateWindowW(L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), m_hInst, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE); return h;
        };
    auto addEdit = [hFont, this](HWND parent, const wchar_t* init, int x, int y, int w, int height, int id, bool readonly = false) {
        HWND h = CreateWindowW(L"EDIT", init, WS_CHILD | WS_VISIBLE | WS_BORDER | (readonly ? ES_READONLY : 0), x, y, w, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), m_hInst, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE); return h;
        };

    // GroupBoxes
    CreateWindowExW(0, L"BUTTON", L"Connexion Série", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 10, 10, 520, 65, m_hwnd, nullptr, m_hInst, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Paramètres PWM", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 10, 85, 520, 240, m_hwnd, nullptr, m_hInst, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Sécurité", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 10, 335, 520, 75, m_hwnd, nullptr, m_hInst, nullptr);

    // Connexion
    m_hComboPorts = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 30, 32, 150, 200, m_hwnd, (HMENU)2000, m_hInst, nullptr);
    SendMessage(m_hComboPorts, WM_SETFONT, (WPARAM)hFont, TRUE);
    RefreshPortList();
    m_hBtnConnect = addBtn(m_hwnd, L"Connecter", 190, 32, 110, 28, ID_CONNECT);

    // PWM
    addStatic(m_hwnd, L"Fréquence (Hz):", 30, 107, 120, 20);
    m_hEditFreq = addEdit(m_hwnd, L"120000", 30, 127, 90, 26, IDC_FREQ_EDIT);
    m_hBtnFreqApply = addBtn(m_hwnd, L"Appliquer", 130, 127, 90, 26, IDC_FREQ_APPLY); // ✅ Handle sauvegardé

    addStatic(m_hwnd, L"Rapport cyclique:", 30, 167, 120, 20);
    m_hSliderDuty = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_TOOLTIPS, 30, 187, 380, 35, m_hwnd, (HMENU)IDC_DUTY_SLIDER, m_hInst, nullptr);
    SendMessage(m_hSliderDuty, TBM_SETRANGEMIN, TRUE, 0); SendMessage(m_hSliderDuty, TBM_SETRANGEMAX, TRUE, 100);
    SendMessage(m_hSliderDuty, TBM_SETPOS, TRUE, 50); SendMessage(m_hSliderDuty, TBM_SETTICFREQ, 10, 0);
    m_hLabelDuty = CreateWindowW(L"STATIC", L"50%", WS_CHILD | WS_VISIBLE, 420, 191, 50, 25, m_hwnd, (HMENU)IDC_DUTY_LABEL, m_hInst, nullptr);
    SendMessage(m_hLabelDuty, WM_SETFONT, (WPARAM)hFont, TRUE);

    addStatic(m_hwnd, L"Dead Time (µs):", 30, 235, 120, 20);
    m_hEditDead = addEdit(m_hwnd, L"0.3", 30, 257, 90, 26, IDC_DEAD_EDIT);
    m_hBtnDeadApply = addBtn(m_hwnd, L"Appliquer", 130, 257, 90, 26, IDC_DEAD_APPLY); // ✅ Handle sauvegardé

    // Sécurité
    m_hBtnEStop = CreateWindowW(L"BUTTON", L"🛑 ARRÊT D'URGENCE", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 30, 360, 240, 42, m_hwnd, (HMENU)IDC_ESTOP_BTN, m_hInst, nullptr);
    SendMessage(m_hBtnEStop, WM_SETFONT, (WPARAM)hFont, TRUE);
    m_hBtnReset = CreateWindowW(L"BUTTON", L"🔄 Réarmement", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 280, 360, 150, 42, m_hwnd, (HMENU)IDC_RESET_BTN, m_hInst, nullptr);
    SendMessage(m_hBtnReset, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Graphique
    m_hGraph = CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | SS_OWNERDRAW, 10, 420, 520, 180, m_hwnd, (HMENU)IDC_GRAPH_AREA, m_hInst, nullptr);
    CreateWindowW(L"STATIC", L" PWM : CH1(Vert) | CH2(Rouge) | Dead Time = zones grises", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 605, 520, 18, m_hwnd, nullptr, m_hInst, nullptr);

    // Log Principal
    HFONT hConsole = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Consolas");
    m_hLogBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 10, 630, 520, 50, m_hwnd, (HMENU)IDC_LOG_BOX, m_hInst, nullptr);
    SendMessage(m_hLogBox, WM_SETFONT, (WPARAM)hConsole, TRUE);

    // ✅ Trace COM Brute
    CreateWindowW(L"STATIC", L"Trace COM:", WS_CHILD | WS_VISIBLE, 10, 685, 80, 18, m_hwnd, nullptr, m_hInst, nullptr);
    m_hChkRawTrace = CreateWindowW(L"BUTTON", L"Afficher", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 90, 683, 80, 22, m_hwnd, (HMENU)5001, m_hInst, nullptr);
    SendMessage(m_hChkRawTrace, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hRawLogBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 10, 705, 520, 90, m_hwnd, (HMENU)5002, m_hInst, nullptr);
    HFONT hTiny = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Consolas");
    SendMessage(m_hRawLogBox, WM_SETFONT, (WPARAM)hTiny, TRUE);

    // Status Bar
    m_hStatusBar = CreateWindowW(STATUSCLASSNAME, L"Déconnecté", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, m_hwnd, (HMENU)IDC_STATUS_BAR, m_hInst, nullptr);

    EnableControls(false);
}

void MainWindow::LogRawTx(const std::string& data) {
    if (!m_rawTraceEnabled || !m_hRawLogBox) return;
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t ts[32]; swprintf_s(ts, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::wstring wdata(data.begin(), data.end());
    std::wstring line = std::wstring(ts) + L"📤 TX: \"" + wdata + L"\"\r\n";
    int len = GetWindowTextLengthW(m_hRawLogBox);
    SendMessageW(m_hRawLogBox, EM_SETSEL, len, len);
    SendMessageW(m_hRawLogBox, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    SendMessage(m_hRawLogBox, WM_VSCROLL, SB_BOTTOM, 0);
}

void MainWindow::LogRawRx(const std::string& data) {
    if (!m_rawTraceEnabled || !m_hRawLogBox) return;
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t ts[32]; swprintf_s(ts, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::wstring wdata(data.begin(), data.end());
    std::wstring line = std::wstring(ts) + L"📥 RX: \"" + wdata + L"\"\r\n";
    int len = GetWindowTextLengthW(m_hRawLogBox);
    SendMessageW(m_hRawLogBox, EM_SETSEL, len, len);
    SendMessageW(m_hRawLogBox, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    SendMessage(m_hRawLogBox, WM_VSCROLL, SB_BOTTOM, 0);
}

void MainWindow::UpdateGraph() {
    if (!m_hGraph) return;
    InvalidateRect(m_hGraph, nullptr, TRUE);
    UpdateWindow(m_hGraph);
}

void MainWindow::DrawGraph(HDC hdc) {
    RECT rect; GetClientRect(m_hGraph, &rect);
    int width = rect.right;
    int height = rect.bottom;

    HBRUSH hBrushBg = CreateSolidBrush(RGB(20, 20, 20));
    FillRect(hdc, &rect, hBrushBg);
    DeleteObject(hBrushBg);

    double periodUs = 1000000.0 / m_currentFreq;
    double pulseWidthUs = periodUs * (m_currentDuty / 100.0);
    double deadTimeUs = m_currentDeadTime;
    double totalTimeUs = periodUs * 2.0;
    double pxPerUs = (double)width / totalTimeUs;

    HPEN hPenGrid = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPenGrid);
    MoveToEx(hdc, 0, height / 2, nullptr);
    LineTo(hdc, width, height / 2);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPenGrid);

    HPEN hPenCh1 = CreatePen(PS_SOLID, 3, RGB(0, 255, 0));
    HPEN hPenCh2 = CreatePen(PS_SOLID, 3, RGB(255, 50, 50));

    int yHigh = height / 4;
    int yLow = (height * 3) / 4;

    // CH1 (Vert)
    SelectObject(hdc, hPenCh1);
    MoveToEx(hdc, 0, yHigh, nullptr);
    int x1 = (int)((pulseWidthUs - deadTimeUs) * pxPerUs);
    LineTo(hdc, x1, yHigh); LineTo(hdc, x1, yLow);
    int x2 = (int)(periodUs * pxPerUs);
    LineTo(hdc, x2, yLow);
    int x3 = (int)((periodUs + deadTimeUs) * pxPerUs);
    LineTo(hdc, x3, yLow);
    int x4 = (int)((periodUs + periodUs) * pxPerUs);
    LineTo(hdc, x4, yLow);

    // CH2 (Rouge - Décalé par Dead Time)
    SelectObject(hdc, hPenCh2);
    MoveToEx(hdc, 0, yLow, nullptr);
    int x_dead_start = (int)(deadTimeUs * pxPerUs);
    LineTo(hdc, x_dead_start, yLow);
    int x_ch2_on = (int)((periodUs + pulseWidthUs - deadTimeUs) * pxPerUs);
    LineTo(hdc, x_dead_start, yHigh); LineTo(hdc, x_ch2_on, yHigh);
    LineTo(hdc, x2, yLow); LineTo(hdc, x4, yLow);

    DeleteObject(hPenCh1);
    DeleteObject(hPenCh2);
}

void MainWindow::RefreshPortList() {
    SendMessage(m_hComboPorts, CB_RESETCONTENT, 0, 0);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        for (DWORD i = 0, nS = 256, nD = 256; ; ++i) {
            wchar_t n[256], d[256]; nS = nD = 256;
            if (RegEnumValueW(hKey, i, n, &nS, nullptr, nullptr, (BYTE*)d, &nD) != ERROR_SUCCESS) break;
            SendMessageW(m_hComboPorts, CB_ADDSTRING, 0, (LPARAM)d);
        }
        RegCloseKey(hKey);
    }
    int c = static_cast<int>(SendMessage(m_hComboPorts, CB_GETCOUNT, 0, 0));
    if (c > 0) SendMessage(m_hComboPorts, CB_SETCURSEL, 0, 0);
}

void MainWindow::OnConnect() {
    if (m_serial.IsConnected()) m_serial.Disconnect();
    else {
        wchar_t port[64] = { 0 };
        SendMessageW(m_hComboPorts, CB_GETLBTEXT, SendMessage(m_hComboPorts, CB_GETCURSEL, 0, 0), (LPARAM)port);
        if (m_serial.Connect(WideStringToString(port))) {
            LogMessage(L"✅ Connecté");
            m_serial.SendCommand(Protocol::MakeStatusCmd()); // ✅ Avec \r\n
        }
        else {
            LogMessage(L"❌ Échec connexion");
            MessageBoxW(m_hwnd, L"Impossible d'ouvrir le port COM", L"Erreur", MB_ICONERROR);
        }
    }
}

void MainWindow::OnEStop() {
    if (MessageBoxW(m_hwnd, L"Confirmer l'arrêt d'urgence ?", L"⚠️ E-STOP", MB_YESNO | MB_ICONWARNING) == IDYES) {
        m_serial.SendCommand(Protocol::MakeEStopCmd()); // ✅ Avec \r\n
        LogMessage(L"🛑 E-STOP envoyé");
    }
}

void MainWindow::OnReset() {
    m_serial.SendCommand(Protocol::MakeResetCmd()); // ✅ Avec \r\n
    LogMessage(L"🔄 Réarmement envoyé");
}

void MainWindow::OnApplyFreq() {
    wchar_t t[32] = { 0 }; GetWindowTextW(m_hEditFreq, t, _countof(t));
    try {
        uint32_t f = std::stoul(WideStringToString(t));

        // ✅ Plage corrigée pour NUCLEO-H723ZG : 120kHz - 250kHz
        if (f >= 120000 && f <= 250000) {
            m_currentFreq = f;

            // ✅ ENVOI ATOMIQUE des 3 paramètres en une seule commande
            m_serial.SendCommand(Protocol::MakePwmConfigCmd(
                m_currentFreq,
                m_currentDuty,
                m_currentDeadTime
            ));

            LogMessage((L"📡 PWM Config: F=" + std::to_wstring(f) +
                L"Hz, D=" + std::to_wstring(m_currentDuty) +
                L"%, DT=" + std::to_wstring(m_currentDeadTime) + L"µs").c_str());
            UpdateGraph();
        }
        else {
            LogMessage(L"❌ Plage valide : 120kHz - 250kHz");
        }
    }
    catch (...) { LogMessage(L"❌ Fréquence invalide"); }
}

void MainWindow::OnApplyDeadTime() {
    wchar_t t[32] = { 0 }; GetWindowTextW(m_hEditDead, t, _countof(t));
    try {
        float d = std::stof(WideStringToString(t));
        if (d >= 0.1f && d <= 1.0f) {  // ✅ Plage 100ns - 1000ns
            m_currentDeadTime = d;  // ✅ Sauvegarde pour le graphique

            // ✅ Envoi atomique
            m_serial.SendCommand(Protocol::MakePwmConfigCmd(
                m_currentFreq,
                m_currentDuty,
                m_currentDeadTime
            ));

            LogMessage((L"📡 DeadTime: " + std::to_wstring(d * 1000) + L" ns").c_str());
            UpdateGraph();
        }
        else {
            LogMessage(L"❌ Plage : 0.1µs - 1.0µs");
        }
    }
    catch (...) { LogMessage(L" Dead Time invalide"); }
}

void MainWindow::OnDutyChanged(int v) {
    wchar_t txt[16]; swprintf_s(txt, L"%d%%", v);
    SetWindowTextW(m_hLabelDuty, txt);

    m_currentDuty = static_cast<uint8_t>(v);  // ✅ Sauvegarde

    // ✅ Envoi atomique
    m_serial.SendCommand(Protocol::MakePwmConfigCmd(
        m_currentFreq,
        m_currentDuty,
        m_currentDeadTime
    ));

    LogMessage((L"📡 Duty: " + std::to_wstring(v) + L"%").c_str());
    UpdateGraph();
}

void MainWindow::OnSerialData(const std::string& line) {
    if (line == Protocol::RESP_OK) {}
    else if (line == Protocol::RESP_ERR) LogMessage(L"❌ Commande invalide (MCU)");
    else if (line == Protocol::RESP_ESTOP) {
        LogMessage(L"🛑 MCU: E-STOP activé");

        // Désactiver les contrôles de commande PWM
        EnableWindow(m_hBtnEStop, FALSE);
        EnableWindow(m_hSliderDuty, FALSE);
        EnableWindow(m_hEditFreq, FALSE);
        EnableWindow(m_hBtnFreqApply, FALSE);
        EnableWindow(m_hEditDead, FALSE);
        EnableWindow(m_hBtnDeadApply, FALSE);

        // ✅ ACTIVER le bouton Réarmement pour permettre de sortir de l'état d'urgence
        EnableWindow(m_hBtnReset, TRUE);
    }
    else if (line == Protocol::RESP_RESET) {
        LogMessage(L"🔄 MCU: Réarmé");

        // Réactiver tous les contrôles
        EnableWindow(m_hBtnEStop, TRUE);
        EnableWindow(m_hSliderDuty, TRUE);
        EnableWindow(m_hEditFreq, TRUE);
        EnableWindow(m_hBtnFreqApply, TRUE);
        EnableWindow(m_hEditDead, TRUE);
        EnableWindow(m_hBtnDeadApply, TRUE);
        EnableWindow(m_hBtnReset, TRUE);
    }

    else if (line.find(Protocol::RESP_STATUS_PREFIX) == 0) {
        auto s = Protocol::ParseStatus(line);
        if (s.valid) {
            wchar_t b[256];
            swprintf_s(b, L"📊 F:%luHz D:%d%% T:%.1fµs E-STOP:%s", s.frequency_hz, s.duty_percent, s.deadtime_us, s.estop_active ? L"OUI" : L"NON");
            UpdateStatus(b);
        }
    }
    else {
        std::wstring wline(line.begin(), line.end());
        LogMessage((L"  " + wline).c_str());
    }
}

void MainWindow::OnConnectionChanged(bool c) {
    EnableControls(c);
    UpdateStatus(c ? L"Connecté" : L"Déconnecté");
    SetWindowTextW(m_hBtnConnect, c ? L"Déconnecter" : L"Connecter");
    LogMessage(c ? L"🔗 Connecté" : L"🔌 Déconnecté");
    if (c) UpdateGraph();
}

void MainWindow::LogMessage(const wchar_t* msg) {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t ts[32]; swprintf_s(ts, L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::wstring full = ts + std::wstring(msg) + L"\r\n";
    int len = GetWindowTextLengthW(m_hLogBox);
    SendMessageW(m_hLogBox, EM_SETSEL, len, len);
    SendMessageW(m_hLogBox, EM_REPLACESEL, FALSE, (LPARAM)full.c_str());
    SendMessage(m_hLogBox, WM_VSCROLL, SB_BOTTOM, 0);
}

void MainWindow::UpdateStatus(const wchar_t* t) {
    SetWindowTextW(m_hStatusBar, t);
}

void MainWindow::EnableControls(bool connected) {
    // Le bouton Connect/Disconnect est TOUJOURS actif
    EnableWindow(m_hBtnConnect, TRUE);

    // La ComboBox des ports est active SEULEMENT si déconnecté
    EnableWindow(m_hComboPorts, !connected);

    // Les contrôles de commande sont actifs SEULEMENT si connecté
    EnableWindow(m_hEditFreq, connected);
    EnableWindow(m_hBtnFreqApply, connected);

    EnableWindow(m_hSliderDuty, connected);

    EnableWindow(m_hEditDead, connected);
    EnableWindow(m_hBtnDeadApply, connected);

    EnableWindow(m_hBtnEStop, connected);
    EnableWindow(m_hBtnReset, connected); // ✅ Le bouton Reset suit l'état de connexion

    // La zone de log est toujours active
    EnableWindow(m_hLogBox, TRUE);
}

std::string MainWindow::WideStringToString(const wchar_t* w) {
    if (!w) return {};
    int s = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string r(s - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, r.data(), s, nullptr, nullptr);
    return r;
}