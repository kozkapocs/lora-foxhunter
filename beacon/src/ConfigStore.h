#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SYSID_MAX_LEN 8

// All beacon configuration parameters.
// Fields with defaults (freq, bw, sf, cr, tx_count, tx_interval_s, period_s)
// are pre-filled by config_init_defaults(). Only system_id and fox_num are
// mandatory — config_is_valid() fails if either is unset.
struct BeaconConfig {
    float   freq;                            // MHz  (default 869.45)
    float   bw;                              // kHz  (default 125.0)
    char    system_id[SYSID_MAX_LEN + 1];   // null-terminated, 1-8 printable chars
    uint8_t fox_num;                         // 1-255  (0 = not set → invalid)
    uint8_t tx_count;                        // packets per burst  (default 10)
    uint8_t tx_interval_s;                   // seconds between packets (default 1)
    uint8_t period_s;                        // full cycle duration in s (default 20)
    uint8_t sf;                              // spreading factor 5-12 (default 9)
    uint8_t cr;                              // coding rate 5-8      (default 5)
    int8_t  tx_power;                        // TX power in dBm, -9 to 22 (default 18)
};

// Fill all fields with factory defaults.
// system_id is left empty and fox_num is set to 0 (→ config_is_valid returns false).
void config_init_defaults(BeaconConfig &cfg);

// Load config from persistent storage into cfg.
// Returns true on success, false if no stored config was found (cfg unchanged).
bool config_load(BeaconConfig &cfg);

// Save cfg to persistent storage.
// Returns true on success.
bool config_save(const BeaconConfig &cfg);

// Returns true only when system_id and fox_num are both set to valid values.
bool config_is_valid(const BeaconConfig &cfg);
