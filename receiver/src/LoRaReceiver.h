#pragma once

#include "ConfigStore.h"
#include <RadioLib.h>
#include <string.h>

// Represents the result of polling the radio for a received packet.
struct RxResult {
    bool    valid;                         // true if a fully parsed packet was received
    float   rssi;                          // signal strength in dBm
    char    system_id[SYSID_MAX_LEN + 1]; // null-terminated system identifier
    uint8_t fox_num;                       // beacon number (1-255)
};

// Non-blocking LoRa receiver.
// Uses RadioLib interrupt-driven receive so the MCU is never blocked waiting.
class LoRaReceiver {
public:
    LoRaReceiver(const ReceiverConfig &cfg, SX1262 &radio);

    // Initialise radio in continuous receive mode.
    // Returns true on success.
    bool begin();

    // Call every loop iteration.
    // Returns an RxResult; valid=true only when a new complete packet arrived.
    // Clears the interrupt flag so each packet is reported exactly once.
    RxResult tick();

private:
    const ReceiverConfig &cfg_;
    SX1262               &radio_;
};
