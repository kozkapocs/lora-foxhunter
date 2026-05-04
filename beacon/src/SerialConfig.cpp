#include "SerialConfig.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <InternalFileSystem.h>
#include <nrfx.h>  // NRF_SPIM0..3 peripheral register pointers

SerialConfig::SerialConfig(BeaconConfig &cfg, Stream &serial)
    : cfg_(cfg), serial_(serial) {}

void SerialConfig::tick() {
    while (serial_.available()) {
        char c = (char)serial_.read();
        if (c == '\r') continue;  // ignore CR
        if (c == '\n') {
            line_buf_[line_len_] = '\0';
            if (line_len_ > 0) {
                process_line();
            }
            line_len_ = 0;
        } else {
            if (line_len_ < sizeof(line_buf_) - 1) {
                line_buf_[line_len_++] = c;
            }
        }
    }
}

bool SerialConfig::wasConfigSaved() {
    if (saved_flag_) {
        saved_flag_ = false;
        return true;
    }
    return false;
}

// Convert a string to uppercase in-place.
static void str_toupper(char *s) {
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

void SerialConfig::process_line() {
    // Make a mutable uppercase copy for command matching.
    char upper[64];
    strncpy(upper, line_buf_, sizeof(upper) - 1);
    upper[sizeof(upper) - 1] = '\0';
    str_toupper(upper);

    // Tokenise: first word is the command verb.
    char *cmd = strtok(upper, " ");
    if (!cmd) return;

    if (strcmp(cmd, "SET") == 0) {
        char *field = strtok(nullptr, " ");
        char *value = strtok(nullptr, " ");
        if (!field || !value) {
            serial_.print("ERR: SET requires field and value\r\n");
            return;
        }
        handle_set(field, value);

    } else if (strcmp(cmd, "GET") == 0) {
        print_all();

    } else if (strcmp(cmd, "SAVE") == 0) {
        if (!config_is_valid(cfg_)) {
            serial_.print("ERR: cannot save — SYSID and FOX must be set first\r\n");
            return;
        }
        // Validate burst-fits-in-period: last packet at (tx_count-1)*tx_interval_s
        if ((uint16_t)(cfg_.tx_count - 1) * cfg_.tx_interval_s >= cfg_.period_s) {
            serial_.print("ERR: burst does not fit in period — reduce TXCOUNT/TXINT or increase PERIOD\r\n");
            return;
        }
        if (config_save(cfg_)) {
            serial_.print("OK\r\n");
            saved_flag_ = true;
        } else {
            serial_.print("ERR: flash write failed\r\n");
        }

    } else if (strcmp(cmd, "RESET") == 0) {
        using namespace Adafruit_LittleFS_Namespace;
        InternalFS.remove("/foxhunter.cfg");
        serial_.print("OK — config erased, rebooting\r\n");
        serial_.flush();
        delay(100);
        NVIC_SystemReset();

    } else if (strcmp(cmd, "DIAG") == 0) {
        // Print hardware diagnostics on demand
        extern const uint32_t g_ADigitalPinMap[];
        serial_.print("PIN MAP: SCK=");  serial_.print(g_ADigitalPinMap[8]);
        serial_.print(" MISO=");         serial_.print(g_ADigitalPinMap[9]);
        serial_.print(" MOSI=");         serial_.print(g_ADigitalPinMap[10]);
        serial_.print(" NSS=");          serial_.print(g_ADigitalPinMap[4]);
        serial_.print(" RST=");          serial_.print(g_ADigitalPinMap[2]);
        serial_.print(" BUSY=");         serial_.println(g_ADigitalPinMap[3]);

        struct { const char *name; NRF_SPIM_Type *r; } spims[] = {
            {"SPIM0", NRF_SPIM0}, {"SPIM1", NRF_SPIM1},
            {"SPIM2", NRF_SPIM2}, {"SPIM3", NRF_SPIM3},
        };
        for (auto &s : spims) {
            serial_.print(s.name);
            serial_.print(": EN="); serial_.print(s.r->ENABLE);
            serial_.print(" SCK=0x"); serial_.print(s.r->PSEL.SCK, HEX);
            serial_.print(" MOSI=0x"); serial_.print(s.r->PSEL.MOSI, HEX);
            serial_.print(" MISO=0x"); serial_.println(s.r->PSEL.MISO, HEX);
        }

        serial_.print("BUSY pin now: "); serial_.println(digitalRead(g_ADigitalPinMap[3]));
        serial_.print("OK\r\n");

    } else {
        serial_.print("ERR: unknown command\r\n");
    }
}

void SerialConfig::handle_set(char *field, char *value) {
    if (strcmp(field, "SYSID") == 0) {
        // value is already upper-cased; restore original case from line_buf_
        // by re-extracting from the original buffer (case-sensitive SYSID value)
        // Find the SYSID value in the original (non-uppercased) buffer.
        const char *orig = line_buf_;
        // Skip "SET SYSID " prefix (10 chars), tolerating variable spacing.
        while (*orig == ' ' || *orig == '\t') ++orig;                   // skip leading space
        while (*orig && *orig != ' ' && *orig != '\t') ++orig;          // skip "SET"
        while (*orig == ' ' || *orig == '\t') ++orig;                   // skip space
        while (*orig && *orig != ' ' && *orig != '\t') ++orig;          // skip "SYSID"
        while (*orig == ' ' || *orig == '\t') ++orig;                   // skip space
        // orig now points to the raw (original-case) SYSID value
        size_t len = strlen(orig);
        if (len == 0 || len > SYSID_MAX_LEN) {
            serial_.print("ERR: SYSID must be 1-8 characters\r\n");
            return;
        }
        for (size_t i = 0; i < len; ++i) {
            if (!isprint((unsigned char)orig[i]) || orig[i] == ' ') {
                serial_.print("ERR: SYSID must not contain spaces or non-printable chars\r\n");
                return;
            }
        }
        memset(cfg_.system_id, 0, sizeof(cfg_.system_id));
        strncpy(cfg_.system_id, orig, SYSID_MAX_LEN);
        serial_.print("OK\r\n");

    } else if (strcmp(field, "FOX") == 0) {
        int v = atoi(value);
        if (v < 1 || v > 255) {
            serial_.print("ERR: FOX must be 1-255\r\n");
            return;
        }
        cfg_.fox_num = (uint8_t)v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "TXCOUNT") == 0) {
        int v = atoi(value);
        if (v < 1 || v > 100) {
            serial_.print("ERR: TXCOUNT must be 1-100\r\n");
            return;
        }
        cfg_.tx_count = (uint8_t)v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "TXINT") == 0) {
        int v = atoi(value);
        if (v < 1 || v > 255) {
            serial_.print("ERR: TXINT must be 1-255 (seconds)\r\n");
            return;
        }
        cfg_.tx_interval_s = (uint8_t)v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "PERIOD") == 0) {
        int v = atoi(value);
        if (v < 1 || v > 255) {
            serial_.print("ERR: PERIOD must be 1-255 (seconds)\r\n");
            return;
        }
        cfg_.period_s = (uint8_t)v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "FREQ") == 0) {
        float v = atof(value);
        if (v < 150.0f || v > 960.0f) {
            serial_.print("ERR: FREQ out of range (150-960 MHz)\r\n");
            return;
        }
        cfg_.freq = v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "BW") == 0) {
        float v = atof(value);
        // Valid RadioLib SX1262 bandwidths in kHz
        const float valid_bw[] = {7.8f, 10.4f, 15.6f, 20.8f, 31.25f, 41.7f, 62.5f, 125.0f, 250.0f, 500.0f};
        bool ok = false;
        for (size_t i = 0; i < sizeof(valid_bw) / sizeof(valid_bw[0]); ++i) {
            if (fabsf(v - valid_bw[i]) < 0.1f) { ok = true; break; }
        }
        if (!ok) {
            serial_.print("ERR: BW must be one of: 7.8 10.4 15.6 20.8 31.25 41.7 62.5 125 250 500\r\n");
            return;
        }
        cfg_.bw = v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "SF") == 0) {
        int v = atoi(value);
        if (v < 5 || v > 12) {
            serial_.print("ERR: SF must be 5-12\r\n");
            return;
        }
        cfg_.sf = (uint8_t)v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "CR") == 0) {
        int v = atoi(value);
        if (v < 5 || v > 8) {
            serial_.print("ERR: CR must be 5-8\r\n");
            return;
        }
        cfg_.cr = (uint8_t)v;
        serial_.print("OK\r\n");

    } else if (strcmp(field, "TXPOWER") == 0) {
        int v = atoi(value);
        if (v < -9 || v > 22) {
            serial_.print("ERR: TXPOWER must be -9 to 22 (dBm)\r\n");
            return;
        }
        cfg_.tx_power = (int8_t)v;
        serial_.print("OK\r\n");

    } else {
        serial_.print("ERR: unknown field\r\n");
    }
}

void SerialConfig::print_all() {
    serial_.print("--- FoxHunter Beacon Config ---\r\n");
    if (config_is_valid(cfg_)) {
        serial_.print("STATUS: OK\r\n");
    } else {
        serial_.print("STATUS: NO CONFIG (set SYSID and FOX, then SAVE)\r\n");
    }
    serial_.print("SYSID:   "); serial_.println(cfg_.system_id[0] ? cfg_.system_id : "(not set)");
    serial_.print("FOX:     ");
    if (cfg_.fox_num) serial_.println(cfg_.fox_num);
    else serial_.print("(not set)\r\n");
    serial_.print("TXCOUNT: "); serial_.println(cfg_.tx_count);
    serial_.print("TXINT:   "); serial_.println(cfg_.tx_interval_s);
    serial_.print("PERIOD:  "); serial_.println(cfg_.period_s);
    serial_.print("FREQ:    "); serial_.println(cfg_.freq, 3);
    serial_.print("BW:      "); serial_.println(cfg_.bw, 2);
    serial_.print("SF:      "); serial_.println(cfg_.sf);
    serial_.print("CR:      "); serial_.println(cfg_.cr);
    serial_.print("TXPOWER: "); serial_.println(cfg_.tx_power);
    serial_.print("-------------------------------\r\n");
}
