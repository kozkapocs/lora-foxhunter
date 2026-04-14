#include "LoRaBeacon.h"
#include <string.h>

// XIAO nRF52840 built-in LEDs (active LOW)
#define LED_TX 12  // Blue — lit briefly during each transmission

LoRaBeacon::LoRaBeacon(const BeaconConfig &cfg, SX1262 &radio, Stream &serial)
    : cfg_(cfg), radio_(radio), serial_(serial) {}

void LoRaBeacon::begin() {
    cycle_start_ms_ = millis();
    burst_idx_      = 0;
    total_tx_       = 0;
    started_        = true;
}

void LoRaBeacon::tick() {
    if (!started_) return;

    uint32_t elapsed = millis() - cycle_start_ms_;

    if (burst_idx_ < cfg_.tx_count) {
        // Still in the burst phase: send the next packet when its slot arrives.
        uint32_t next_tx_ms = (uint32_t)burst_idx_ * (uint32_t)cfg_.tx_interval_s * 1000UL;
        if (elapsed >= next_tx_ms) {
            send_packet();
            burst_idx_++;
        }
    } else {
        // Burst done, waiting for the next cycle.
        if (elapsed >= (uint32_t)cfg_.period_s * 1000UL) {
            cycle_start_ms_ = millis();
            burst_idx_ = 0;
        }
    }
}

void LoRaBeacon::send_packet() {
    // Build the 9-byte payload: [ system_id (8 bytes, null-padded) | fox_num (1 byte) ]
    uint8_t payload[9];
    memset(payload, 0, sizeof(payload));
    strncpy(reinterpret_cast<char *>(payload), cfg_.system_id, 8);
    payload[8] = cfg_.fox_num;

    // Visual TX indicator
    digitalWrite(LED_TX, LOW);

    int16_t state = radio_.transmit(payload, sizeof(payload));

    digitalWrite(LED_TX, HIGH);

    total_tx_++;

    // Status output on serial (useful for on-site verification)
    if (state == RADIOLIB_ERR_NONE) {
        serial_.print("TX #");
        serial_.print(total_tx_);
        serial_.print(" [");
        serial_.print(cfg_.system_id);
        serial_.print("/");
        serial_.print(cfg_.fox_num);
        serial_.print("] RSSI floor: ");
        serial_.print(radio_.getRSSI());
        serial_.print(" dBm\r\n");
    } else {
        serial_.print("TX ERROR: ");
        serial_.println(state);
    }
}
