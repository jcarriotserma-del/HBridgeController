#pragma once
#include <string>
#include <cstdint>
#include <cstring>

namespace Protocol {
    constexpr char CMD_FREQ[] = "F";
    constexpr char CMD_DUTY[] = "D";
    constexpr char CMD_DEADTIME[] = "T";
    constexpr char CMD_ESTOP[] = "E";
    constexpr char CMD_RESET[] = "R";
    constexpr char CMD_STATUS[] = "S";
    constexpr char RESP_OK[] = "OK";
    constexpr char RESP_ERR[] = "ERR";
    constexpr char RESP_ESTOP[] = "ESTOP";
    constexpr char RESP_RESET[] = "RESET";
    constexpr char RESP_STATUS_PREFIX[] = "STATUS:";
    constexpr char CMD_TERMINATOR[] = "\r\n";

    inline std::string MakeFreqCmd(uint32_t hz) { return CMD_FREQ + std::to_string(hz) + CMD_TERMINATOR; }
    inline std::string MakeDutyCmd(uint8_t percent) { return CMD_DUTY + std::to_string(percent) + CMD_TERMINATOR; }
    inline std::string MakeDeadTimeCmd(float us) { return CMD_DEADTIME + std::to_string(us) + CMD_TERMINATOR; }

    struct StatusData {
        uint32_t frequency_hz = 0;
        uint8_t duty_percent = 0;
        float deadtime_us = 0.0f;
        bool estop_active = false;
        bool valid = false;
    };

    inline StatusData ParseStatus(const std::string& line) {
        StatusData s;
        if (line.find(RESP_STATUS_PREFIX) != 0) return s;
        auto data = line.substr(std::strlen(RESP_STATUS_PREFIX));
        size_t pos = 0;
        while (pos < data.size()) {
            auto next = data.find(',', pos);
            auto token = data.substr(pos, next - pos);
            if (token[0] == 'F') s.frequency_hz = std::stoul(token.substr(1));
            else if (token[0] == 'D') s.duty_percent = static_cast<uint8_t>(std::stoul(token.substr(1)));
            else if (token[0] == 'T') s.deadtime_us = std::stof(token.substr(1));
            else if (token.find("ESTOP:") == 0) s.estop_active = (token[6] == '1');
            pos = (next == std::string::npos) ? data.size() : next + 1;
        }
        s.valid = true;
        return s;
    }
}