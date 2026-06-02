#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

class SerialController {
public:
    using OnDataReceived = std::function<void(const std::string&)>;
    using OnConnectionChanged = std::function<void(bool)>;
    SerialController(); ~SerialController();
    bool Connect(const std::string& portName, DWORD baudRate = CBR_115200);
    void Disconnect();
    bool IsConnected() const { return m_connected; }
    bool SendCommand(const std::string& cmd);
    void SetOnDataReceived(OnDataReceived cb) { m_onData = cb; }
    void SetOnConnectionChanged(OnConnectionChanged cb) { m_onConn = cb; }
    void RunReadThread(); void StopReadThread();
private:
    HANDLE m_hComm = INVALID_HANDLE_VALUE;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_readThreadRunning{false};
    std::thread m_readThread;
    OnDataReceived m_onData;
    OnConnectionChanged m_onConn;
    std::string m_rxBuffer;
    void ProcessByte(char c);
    bool ConfigurePort(DWORD baudRate);
};
