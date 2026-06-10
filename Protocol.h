#pragma once
#include <string>
#include <cstdint>
#include <cstring>

namespace Protocol {
    // ============================================================
    // 1. COMMANDES & RÉPONSES BRUTES
    // ============================================================
    constexpr char CMD_FREQ[] = "F";
    constexpr char CMD_DUTY[] = "D";
    constexpr char CMD_DEADTIME[] = "T";
    constexpr char CMD_ESTOP[] = "E";
    constexpr char CMD_RESET[] = "R";
    constexpr char CMD_STATUS[] = "S";
    constexpr char CMD_PWM_CONFIG[] = "P";

    constexpr char CMD_TERMINATOR[] = "\r\n";

    constexpr char RESP_OK[] = "OK";
    constexpr char RESP_ERR[] = "ERR";
    constexpr char RESP_ERR_RANGE[] = "ERR_RANGE";
    constexpr char RESP_ESTOP[] = "ESTOP";
    constexpr char RESP_RESET[] = "RESET";
    constexpr char RESP_STATUS_PREFIX[] = "STATUS:";

    // ============================================================
    // 2. HELPERS DE CONSTRUCTION (AJOUTENT AUTOMATIQUEMENT \r\n)
    // ============================================================
    inline std::string MakeFreqCmd(uint32_t hz) {
        return std::string(CMD_FREQ) + std::to_string(hz) + CMD_TERMINATOR;
    }
    inline std::string MakeDutyCmd(uint8_t percent) {
        return std::string(CMD_DUTY) + std::to_string(percent) + CMD_TERMINATOR;
    }
    inline std::string MakeDeadTimeCmd(float us) {
        return std::string(CMD_DEADTIME) + std::to_string(us) + CMD_TERMINATOR;
    }

    inline std::string MakePwmConfigCmd(uint32_t freq_hz, uint8_t duty_percent, float dead_us) {
        return std::string(CMD_PWM_CONFIG) +
            std::to_string(freq_hz) + "," +
            std::to_string(duty_percent) + "," +
            std::to_string(dead_us) +
            CMD_TERMINATOR;
    }

    // Parser pour la réponse (optionnel, si le MCU confirme)
    inline bool ParsePwmConfigAck(const std::string& line) {
        return (line == "OK\r\n" || line == "OK");
    }

    // ✅ NOUVEAUX : Helpers pour commandes mono-caractère (résout le bug CR/LF)
    inline std::string MakeEStopCmd() {
        return std::string(CMD_ESTOP) + CMD_TERMINATOR;
    }
    inline std::string MakeResetCmd() {
        return std::string(CMD_RESET) + CMD_TERMINATOR;
    }
    inline std::string MakeStatusCmd() {
        return std::string(CMD_STATUS) + CMD_TERMINATOR;
    }

    // ============================================================
    // 3. PARSING DU STATUT MCU
    // ============================================================
    struct StatusData {
        uint32_t frequency_hz = 0;
        uint8_t  duty_percent = 0;
        float    deadtime_us = 0.0f;
        bool     estop_active = false;
        bool     valid = false;
    };

    inline StatusData ParseStatus(const std::string& line) {
        StatusData s;
        if (line.find(RESP_STATUS_PREFIX) != 0) return s;

        auto data = line.substr(std::strlen(RESP_STATUS_PREFIX));
        size_t pos = 0;
        while (pos < data.size()) {
            size_t next = data.find(',', pos);
            auto token = data.substr(pos, next - pos);

            if (token[0] == 'F') {
                s.frequency_hz = std::stoul(token.substr(1));
            }
            else if (token[0] == 'D') {
                s.duty_percent = static_cast<uint8_t>(std::stoul(token.substr(1)));
            }
            else if (token[0] == 'T') {
                s.deadtime_us = std::stof(token.substr(1));
            }
            else if (token.find("ESTOP:") == 0) {
                s.estop_active = (token[6] == '1');
            }

            pos = (next == std::string::npos) ? data.size() : next + 1;
        }
        s.valid = true;
        return s;
    }
} // namespace Protocol