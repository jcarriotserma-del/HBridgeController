#include "SerialController.h"
#include "Protocol.h"
#include <chrono>

SerialController::SerialController() = default;
SerialController::~SerialController() { Disconnect(); }

bool SerialController::Connect(const std::string& portName, DWORD baudRate) {
    if (m_connected) return false;
    std::string fullPath = "\\\\.\\" + portName;
    m_hComm = CreateFileA(fullPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (m_hComm == INVALID_HANDLE_VALUE) return false;
    if (!ConfigurePort(baudRate)) { CloseHandle(m_hComm); return false; }
    PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    m_connected = true;
    if (m_onConn) m_onConn(true);
    m_readThreadRunning = true;
    m_readThread = std::thread(&SerialController::RunReadThread, this);
    return true;
}

void SerialController::Disconnect() {
    m_readThreadRunning = false;
    if (m_readThread.joinable()) m_readThread.join();
    if (m_hComm != INVALID_HANDLE_VALUE) { CloseHandle(m_hComm); m_hComm = INVALID_HANDLE_VALUE; }
    if (m_connected) { m_connected = false; if (m_onConn) m_onConn(false); }
}

bool SerialController::SendCommand(const std::string& cmd) {
    if (!m_connected || m_hComm == INVALID_HANDLE_VALUE) return false;

    // ✅ Trace TX avant envoi
    if (m_onRawTx) m_onRawTx(cmd + "\r\n");

    DWORD written = 0;
    OVERLAPPED ov = { 0 };
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    BOOL res = WriteFile(m_hComm, cmd.c_str(), (DWORD)cmd.size(), &written, &ov);
    if (!res && GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(ov.hEvent, INFINITE);
        GetOverlappedResult(m_hComm, &ov, &written, TRUE);
    }
    CloseHandle(ov.hEvent);
    return (written == cmd.size());
}

bool SerialController::ConfigurePort(DWORD baudRate) {
    DCB dcb = { 0 }; dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(m_hComm, &dcb)) return false;
    dcb.BaudRate = baudRate; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE; dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fBinary = TRUE; dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
    if (!SetCommState(m_hComm, &dcb)) return false;
    COMMTIMEOUTS to = { 0 }; to.ReadIntervalTimeout = 50; to.ReadTotalTimeoutMultiplier = 10; to.ReadTotalTimeoutConstant = 1000;
    SetCommTimeouts(m_hComm, &to); return true;
}

void SerialController::RunReadThread() {
    DWORD bytesRead; char buffer[256]; OVERLAPPED ov = { 0 }; ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    while (m_readThreadRunning && m_connected) {
        BOOL res = ReadFile(m_hComm, buffer, sizeof(buffer), &bytesRead, &ov);
        if (!res) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) { WaitForSingleObject(ov.hEvent, 100); GetOverlappedResult(m_hComm, &ov, &bytesRead, FALSE); }
            else if (err == ERROR_OPERATION_ABORTED) break;
            else { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        }
        for (DWORD i = 0; i < bytesRead; ++i) ProcessByte(buffer[i]);
    }
    CloseHandle(ov.hEvent);
}

void SerialController::ProcessByte(char c) {
    //if (c == '\r') return;
    //if (c == '\n') { if (!m_rxBuffer.empty() && m_onData) m_onData(m_rxBuffer); m_rxBuffer.clear(); }
    //else m_rxBuffer += c;


    if (c == '\r') return;
    if (c == '\n') {
        if (!m_rxBuffer.empty()) {
            // ✅ Trace RX avant parsing
            if (m_onRawRx) m_onRawRx(m_rxBuffer + "\n");

            if (m_onData) m_onData(m_rxBuffer);
        }
        m_rxBuffer.clear();
    }
    else {
        m_rxBuffer += c;
    }
}