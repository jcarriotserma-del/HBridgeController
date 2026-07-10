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

    // ✅ Callbacks Trace COM Brute
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

    // Hauteur de la fenêtre
    m_hwnd = CreateWindowExW(0, L"HBridgeWndClass", L"Contrôle Pont en H - STM32 (C++)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 780,
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
        case ID_REFRESH_PORTS: pThis->OnRefreshPorts(); break;  // ✅ NOUVEAU
        case IDC_ESTOP_BTN: pThis->OnEStop(); break;
        case IDC_RESET_BTN: pThis->OnReset(); break;
        case IDC_FREQ_APPLY: pThis->OnApplyFreq(); break;

            // ✅ NOUVEAUX BOUTONS +/- 1kHz
        case IDC_FREQ_MINUS1K: pThis->OnFreqAdjust(-1000); break;
        case IDC_FREQ_PLUS1K:  pThis->OnFreqAdjust(1000); break;
            // ✅ NOUVEAUX CAS POUR +/- 10kHz
        case IDC_FREQ_PLUS:  pThis->OnFreqAdjust(10000); break;
        case IDC_FREQ_MINUS: pThis->OnFreqAdjust(-10000); break;

        case IDC_DEAD_APPLY: pThis->OnApplyDeadTime(); break;
        case ID_EXIT: DestroyWindow(hwnd); break;

            // ✅ Gestion de la checkbox Trace COM (ID 5001)
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

void MainWindow::OnFreqAdjust(int delta) {
    try {
        // 1. Calculer la nouvelle fréquence
        uint32_t newFreq = m_currentFreq + delta;

        // 2. Bloquer aux limites (120kHz - 250kHz)
        if (newFreq < 120000) newFreq = 120000;
        if (newFreq > 250000) newFreq = 250000;

        // 3. Mettre à jour la variable interne
        m_currentFreq = newFreq;

        // 4. ✅ METTRE À JOUR L'AFFICHAGE (C'est l'étape manquante)
        wchar_t txt[32];
        swprintf_s(txt, L"%lu", newFreq);
        SetWindowTextW(m_hEditFreq, txt);

        // 5. Envoyer la commande au STM32 (Comme si on avait cliqué sur Appliquer)
        m_serial.SendCommand(Protocol::MakePwmConfigCmd(
            m_currentFreq,
            m_currentDuty,
            m_currentDeadTime
        ));

        LogMessage((L" Freq ajustée : " + std::to_wstring(newFreq) + L" Hz").c_str());
    }
    catch (...) {
        LogMessage(L"❌ Erreur ajustement fréquence");
    }
}

void MainWindow::OnRefreshPorts() {
    // Mémoriser le port actuellement sélectionné
    wchar_t currentPort[64] = { 0 };
    int selectedIndex = static_cast<int>(SendMessageW(m_hComboPorts, CB_GETCURSEL, 0, 0));
    if (selectedIndex >= 0) {
        SendMessageW(m_hComboPorts, CB_GETLBTEXT, selectedIndex, (LPARAM)currentPort);
    }

    // Rafraîchir la liste
    RefreshPortList();

    // Tenter de resélectionner le même port s'il existe toujours
    if (currentPort[0] != 0) {
        int newIndex = static_cast<int>(SendMessageW(m_hComboPorts, CB_FINDSTRINGEXACT, -1, (LPARAM)currentPort));
        if (newIndex >= 0) {
            SendMessageW(m_hComboPorts, CB_SETCURSEL, newIndex, 0);
        }
    }

    LogMessage(L"🔄 Liste des ports COM rafraîchie");
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

    // ✅ BOUTON RAFRAÎCHIR LES PORTS COM
    HWND m_hBtnRefreshPorts = CreateWindowW(L"BUTTON", L"🔄",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        185, 32, 35, 28, m_hwnd, (HMENU)ID_REFRESH_PORTS, m_hInst, nullptr);
    SendMessage(m_hBtnRefreshPorts, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowTextW(m_hBtnRefreshPorts, L"↻");  // Symbole rafraîchissement

    m_hBtnConnect = addBtn(m_hwnd, L"Connecter", 225, 32, 110, 28, ID_CONNECT);

    /* ---------------- PWM ---------------- */
    addStatic(m_hwnd, L"Fréquence (Hz):", 30, 107, 120, 20);

    // 1. Champ de saisie (X=30, W=90 -> Fin à 120)
    m_hEditFreq = addEdit(m_hwnd, L"180000", 30, 127, 90, 26, IDC_FREQ_EDIT);

    // 2. Bouton Appliquer (X=130, W=75 -> Fin à 205)
    m_hBtnFreqApply = addBtn(m_hwnd, L"Appliquer", 130, 127, 75, 26, IDC_FREQ_APPLY);

    // 3. Bouton -1kHz (X=215, W=60 -> Fin à 275)
    m_hBtnFreqMinus1k = CreateWindowW(L"BUTTON", L"-1kHz", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        215, 127, 60, 26, m_hwnd, (HMENU)IDC_FREQ_MINUS1K, m_hInst, nullptr);
    SendMessage(m_hBtnFreqMinus1k, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 4. Bouton +1kHz (X=285, W=60 -> Fin à 345)
    m_hBtnFreqPlus1k = CreateWindowW(L"BUTTON", L"+1kHz", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        285, 127, 60, 26, m_hwnd, (HMENU)IDC_FREQ_PLUS1K, m_hInst, nullptr);
    SendMessage(m_hBtnFreqPlus1k, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 5. Bouton -10kHz (X=355, W=65 -> Fin à 420)
    m_hBtnFreqMinus = CreateWindowW(L"BUTTON", L"-10kHz", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        355, 127, 65, 26, m_hwnd, (HMENU)IDC_FREQ_MINUS, m_hInst, nullptr);
    SendMessage(m_hBtnFreqMinus, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 6. Bouton +10kHz (X=430, W=65 -> Fin à 495)
    m_hBtnFreqPlus = CreateWindowW(L"BUTTON", L"+10kHz", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        430, 127, 65, 26, m_hwnd, (HMENU)IDC_FREQ_PLUS, m_hInst, nullptr);
    SendMessage(m_hBtnFreqPlus, WM_SETFONT, (WPARAM)hFont, TRUE);


    // Déplacer le bouton Appliquer pour faire de la place
    //m_hBtnFreqApply = addBtn(m_hwnd, L"Appliquer", 280, 127, 90, 26, IDC_FREQ_APPLY);

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

    // Log Principal (y=430, H=80)
    HFONT hConsole = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Consolas");
    m_hLogBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        10, 430, 520, 80, m_hwnd, (HMENU)IDC_LOG_BOX, m_hInst, nullptr);
    SendMessage(m_hLogBox, WM_SETFONT, (WPARAM)hConsole, TRUE);

    // ✅ Trace COM Brute (y=515, H=180) ← AGRANDIE ICI
    CreateWindowW(L"STATIC", L"Trace COM:", WS_CHILD | WS_VISIBLE, 10, 515, 80, 18, m_hwnd, nullptr, m_hInst, nullptr);
    m_hChkRawTrace = CreateWindowW(L"BUTTON", L"Afficher",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        90, 513, 80, 22, m_hwnd, (HMENU)5001, m_hInst, nullptr);
    SendMessage(m_hChkRawTrace, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hRawLogBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        10, 535, 520, 180, m_hwnd, (HMENU)5002, m_hInst, nullptr); // Hauteur passée à 180
    HFONT hTiny = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Consolas");
    SendMessage(m_hRawLogBox, WM_SETFONT, (WPARAM)hTiny, TRUE);

    // Status Bar
    m_hStatusBar = CreateWindowW(STATUSCLASSNAME, L"Déconnecté",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, m_hwnd, (HMENU)IDC_STATUS_BAR, m_hInst, nullptr);

    EnableControls(false);
}

// ✅ Implémentation Trace COM Brute
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
            m_serial.SendCommand(Protocol::MakeStatusCmd());
        }
        else {
            LogMessage(L"❌ Échec connexion");
            MessageBoxW(m_hwnd, L"Impossible d'ouvrir le port COM", L"Erreur", MB_ICONERROR);
        }
    }
}

void MainWindow::OnEStop() {
    if (MessageBoxW(m_hwnd, L"Confirmer l'arrêt d'urgence ?", L"⚠️ E-STOP", MB_YESNO | MB_ICONWARNING) == IDYES) {
        m_serial.SendCommand(Protocol::MakeEStopCmd());
        LogMessage(L"🛑 E-STOP envoyé");
    }
}

void MainWindow::OnReset() {
    m_serial.SendCommand(Protocol::MakeResetCmd());
    LogMessage(L"🔄 Réarmement envoyé");
}

// ✅ COMMANDE ATOMIQUE : Envoie Freq, Duty et DeadTime en une seule trame
void MainWindow::OnApplyFreq() {
    wchar_t t[32] = { 0 }; GetWindowTextW(m_hEditFreq, t, _countof(t));
    try {
        uint32_t f = std::stoul(WideStringToString(t));
        // ✅ Plage corrigée pour NUCLEO-H723ZG : 120kHz - 250kHz
        if (f >= 120000 && f <= 250000) {
            m_currentFreq = f;

            // ✅ ENVOI ATOMIQUE : Tout s'adapte en même temps sur le MCU
            m_serial.SendCommand(Protocol::MakePwmConfigCmd(
                m_currentFreq,
                m_currentDuty,
                m_currentDeadTime
            ));

            LogMessage((L"📡 Config PWM: F=" + std::to_wstring(f) + L"Hz").c_str());
        }
        else {
            LogMessage(L"❌ Plage valide : 120kHz - 250kHz");
        }
    }
    catch (...) { LogMessage(L"❌ Fréquence invalide"); }
}

// ✅ COMMANDE ATOMIQUE
void MainWindow::OnApplyDeadTime() {
    wchar_t t[32] = { 0 }; GetWindowTextW(m_hEditDead, t, _countof(t));
    try {
        float d = std::stof(WideStringToString(t));
        // ✅ Plage corrigée : 0.1µs - 1.0µs
        if (d >= 0.1f && d <= 1.0f) {
            m_currentDeadTime = d;

            // ✅ ENVOI ATOMIQUE
            m_serial.SendCommand(Protocol::MakePwmConfigCmd(
                m_currentFreq,
                m_currentDuty,
                m_currentDeadTime
            ));

            LogMessage((L"📡 DeadTime: " + std::to_wstring(d * 1000) + L" ns").c_str());
        }
        else {
            LogMessage(L"❌ Plage : 0.1µs - 1.0µs");
        }
    }
    catch (...) { LogMessage(L" Dead Time invalide"); }
}

// ✅ COMMANDE ATOMIQUE
void MainWindow::OnDutyChanged(int v) {
    wchar_t txt[16]; swprintf_s(txt, L"%d%%", v);
    SetWindowTextW(m_hLabelDuty, txt);

    m_currentDuty = static_cast<uint8_t>(v);

    // ✅ ENVOI ATOMIQUE
    m_serial.SendCommand(Protocol::MakePwmConfigCmd(
        m_currentFreq,
        m_currentDuty,
        m_currentDeadTime
    ));

    LogMessage((L"📡 Duty: " + std::to_wstring(v) + L"%").c_str());
}

void MainWindow::OnSerialData(const std::string& line) {
    if (line == Protocol::RESP_OK) {}
    else if (line == Protocol::RESP_ERR) LogMessage(L"❌ Commande invalide (MCU)");

    // ✅ GESTION SPÉCIFIQUE E-STOP : Désactive les commandes mais garde Reset actif
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

    // ✅ GESTION DU RETOUR À LA NORMALE
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
        LogMessage((L"📥  " + wline).c_str());
    }
}

void MainWindow::OnConnectionChanged(bool c) {
    EnableControls(c);
    UpdateStatus(c ? L"Connecté" : L"Déconnecté");
    SetWindowTextW(m_hBtnConnect, c ? L"Déconnecter" : L"Connecter");
    LogMessage(c ? L"🔗 Connecté" : L"🔌 Déconnecté");
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
    EnableWindow(m_hBtnConnect, TRUE);
    EnableWindow(m_hComboPorts, !connected);
    EnableWindow(m_hEditFreq, connected);
    EnableWindow(m_hBtnFreqApply, connected);
    // ✅ AJOUT DES NOUVEAUX BOUTONS
    EnableWindow(m_hBtnFreqMinus1k, connected);
    EnableWindow(m_hBtnFreqPlus1k, connected);
    EnableWindow(m_hBtnFreqMinus, connected);
    EnableWindow(m_hBtnFreqPlus, connected);
    EnableWindow(m_hSliderDuty, connected);
    EnableWindow(m_hEditDead, connected);
    EnableWindow(m_hBtnDeadApply, connected);
    EnableWindow(m_hBtnEStop, connected);
    EnableWindow(m_hBtnReset, connected);
    EnableWindow(m_hLogBox, TRUE);
}

std::string MainWindow::WideStringToString(const wchar_t* w) {
    if (!w) return {};
    int s = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string r(s - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, r.data(), s, nullptr, nullptr);
    return r;
}