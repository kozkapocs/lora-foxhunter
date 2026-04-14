#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SYSID_MAX_LEN 8

// All receiver configuration parameters.
struct ReceiverConfig {
    float   freq;                           // MHz  (default 869.45)
    float   bw;                             // kHz  (default 125.0)
    char    system_id[SYSID_MAX_LEN + 1];  // null-terminated, 1-8 printable chars
    uint8_t sf;                             // spreading factor 5-12 (default 9)
    uint8_t cr;                             // coding rate 5-8      (default 5)
};

// Fill all fields with factory defaults.
// system_id is left empty → config_is_valid() returns false until SET SYSID is used.
void config_init_defaults(ReceiverConfig &cfg);

// Load config from persistent storage into cfg.
// Returns true on success, false if no stored config was found (cfg unchanged).
bool config_load(ReceiverConfig &cfg);

// Save cfg to persistent storage.
// Returns true on success.
bool config_save(const ReceiverConfig &cfg);

// Returns true only when system_id is set to a valid (non-empty) value.
bool config_is_valid(const ReceiverConfig &cfg);
