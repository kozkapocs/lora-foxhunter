#include "SerialConfig.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <InternalFileSystem.h>

SerialConfig::SerialConfig(ReceiverConfig &cfg, Stream &serial)
    : cfg_(cfg), serial_(serial) {}

void SerialConfig::tick() {
    while (serial_.available()) {
        char c = (char)serial_.read();
        if (c == '\r') continue;
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

static void str_toupper(char *s) {
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

void SerialConfig::process_line() {
    char upper[64];
    strncpy(upper, line_buf_, sizeof(upper) - 1);
    upper[sizeof(upper) - 1] = '\0';
    str_toupper(upper);

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
            serial_.print("ERR: cannot save — SYSID must be set first\r\n");
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
        InternalFS.remove("/foxhunter_rx.cfg");
        serial_.print("OK — config erased, rebooting\r\n");
        serial_.flush();
        delay(100);
        NVIC_SystemReset();

    } else {
        serial_.print("ERR: unknown command\r\n");
    }
}

void SerialConfig::handle_set(char *field, char *value) {
    if (strcmp(field, "SYSID") == 0) {
        // Restore original case from line_buf_
        const char *orig = line_buf_;
        while (*orig == ' ' || *orig == '\t') ++orig;
        while (*orig && *orig != ' ' && *orig != '\t') ++orig;  // skip SET
        while (*orig == ' ' || *orig == '\t') ++orig;
        while (*orig && *orig != ' ' && *orig != '\t') ++orig;  // skip SYSID
        while (*orig == ' ' || *orig == '\t') ++orig;

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

    } else {
        serial_.print("ERR: unknown field\r\n");
    }
}

void SerialConfig::print_all() {
    serial_.print("SYSID  = ");
    serial_.print(cfg_.system_id[0] ? cfg_.system_id : "(not set)");
    serial_.print("\r\nFREQ   = ");
    serial_.print(cfg_.freq, 3);
    serial_.print(" MHz\r\nBW     = ");
    serial_.print(cfg_.bw, 2);
    serial_.print(" kHz\r\nSF     = ");
    serial_.print(cfg_.sf);
    serial_.print("\r\nCR     = ");
    serial_.print(cfg_.cr);
    serial_.print("\r\n");
}
