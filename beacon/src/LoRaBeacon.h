#pragma once

#include "ConfigStore.h"
#include <RadioLib.h>

// Drives periodic burst transmissions on a pre-initialised SX1262 radio.
//
// Timing model:
//   Each cycle lasts period_s seconds.
//   At the start of each cycle, tx_count packets are sent, one every
//   tx_interval_s seconds (first packet is sent immediately).
//   The remaining time in the cycle is a quiet pause.
//
// Example (tx_count=10, tx_interval_s=1, period_s=20):
//   t=0s   → TX packet 1
//   t=1s   → TX packet 2
//   ...
//   t=9s   → TX packet 10
//   t=10s-20s → silence
//   t=20s  → new cycle begins
class LoRaBeacon {
public:
    LoRaBeacon(const BeaconConfig &cfg, SX1262 &radio, Stream &serial = Serial);

    // Start (or restart) the transmission cycle immediately.
    // Must be called before tick() does anything.
    void begin();

    // Call every loop iteration. Non-blocking except during the ~144 ms
    // blocking radio.transmit() call itself.
    // Does nothing until begin() has been called at least once.
    void tick();

    bool isStarted() const { return started_; }

    // Total packets transmitted since begin() was last called.
    uint32_t getTotalTxCount() const { return total_tx_; }

private:
    bool started_ = false;
    const BeaconConfig &cfg_;
    SX1262             &radio_;
    Stream             &serial_;

    uint32_t cycle_start_ms_ = 0;
    uint8_t  burst_idx_      = 0;
    uint32_t total_tx_       = 0;

    void send_packet();
};
