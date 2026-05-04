#pragma once

#include "ConfigStore.h"
#include <Arduino.h>

// Parses text commands arriving on the USB serial port and updates cfg.
//
// Supported commands (case-insensitive, \r\n or \n terminated):
//   SET SYSID <value>    — system identifier, 1-8 printable non-space chars
//   SET FREQ  <MHz>      — LoRa frequency
//   SET BW    <kHz>      — LoRa bandwidth
//   SET SF    <5-12>     — LoRa spreading factor
//   SET CR    <5-8>      — LoRa coding rate
//   SET BEEP  <ms>       — buzzer beep duration in milliseconds (20-500)
//   SET FREQMIN <Hz>     — buzzer minimum frequency in Hz (50-5000)
//   SET FREQMAX <Hz>     — buzzer maximum frequency in Hz (50-5000)
//   GET ALL              — print all current parameters
//   SAVE                 — persist current config to flash
//   RESET                — erase stored config and reboot
//
// Every command returns "OK\r\n" on success or "ERR: <reason>\r\n" on failure.
// wasConfigSaved() returns true (once) after a successful SAVE command.
class SerialConfig {
public:
    SerialConfig(ReceiverConfig &cfg, Stream &serial = Serial);

    // Call every loop iteration.
    void tick();

    // Returns true once after a successful SAVE, then resets to false.
    bool wasConfigSaved();

private:
    ReceiverConfig &cfg_;
    Stream         &serial_;
    char            line_buf_[64];
    uint8_t         line_len_ = 0;
    bool            saved_flag_ = false;

    void process_line();
    void handle_set(char *field, char *value);
    void print_all();
};
