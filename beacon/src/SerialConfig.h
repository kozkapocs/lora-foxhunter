#pragma once

#include "ConfigStore.h"
#include <Arduino.h>

// Parses text commands arriving on the USB serial port and updates cfg.
//
// Supported commands (case-insensitive, \r\n or \n terminated):
//   SET SYSID <value>    — system identifier, 1-8 printable non-space chars
//   SET FOX   <1-255>    — fox number
//   SET TXCOUNT <1-100>  — packets per burst
//   SET TXINT  <1-255>   — seconds between packets in a burst
//   SET PERIOD <1-255>   — full cycle duration in seconds
//   SET FREQ  <MHz>      — LoRa frequency
//   SET BW    <kHz>      — LoRa bandwidth
//   SET SF    <5-12>     — LoRa spreading factor
//   SET CR    <5-8>      — LoRa coding rate
//   SET TXPOWER <-9-22>  — TX power in dBm
//   GET ALL              — print all current parameters
//   SAVE                 — persist current config to flash
//   RESET                — erase stored config and reboot
//
// Every command returns "OK\r\n" on success or "ERR: <reason>\r\n" on failure.
// wasConfigSaved() returns true (once) after a successful SAVE command.
class SerialConfig {
public:
    SerialConfig(BeaconConfig &cfg, Stream &serial = Serial);

    // Call every loop iteration. Reads available bytes and processes complete lines.
    void tick();

    // Returns true once after a successful SAVE, then resets to false.
    bool wasConfigSaved();

private:
    BeaconConfig &cfg_;
    Stream       &serial_;
    char          line_buf_[64];
    uint8_t       line_len_ = 0;
    bool          saved_flag_ = false;

    void process_line();
    void handle_set(char *field, char *value);
    void print_all();
};
