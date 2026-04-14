#include "LoRaReceiver.h"
#include <string.h>

// Packet layout sent by the beacon (9 bytes total):
//   [0..7]  system_id — null-padded ASCII string (8 bytes)
//   [8]     fox_num   — beacon number (1-255)
static const size_t PACKET_LEN = 9;

// ISR flag set from the DIO1 interrupt (RadioLib callback).
static volatile bool rx_flag = false;

static void rx_isr() {
    rx_flag = true;
}

LoRaReceiver::LoRaReceiver(const ReceiverConfig &cfg, SX1262 &radio)
    : cfg_(cfg), radio_(radio) {}

bool LoRaReceiver::begin() {
    // Try four combinations: TCXO 1.8 V / crystal × DCDC / LDO, same as
    // the beacon firmware, in case the board oscillator config varies.
    struct { float tcxo; bool ldo; } tries[] = {
        { 1.8f, false },  // TCXO on DIO3, DCDC  (Wio Tracker L1 default)
        { 1.8f, true  },  // TCXO on DIO3, LDO
        { 0.0f, false },  // crystal / ext-TCXO, DCDC
        { 0.0f, true  },  // crystal / ext-TCXO, LDO
    };

    int16_t state = RADIOLIB_ERR_UNKNOWN;
    for (auto &t : tries) {
        state = radio_.begin(
            cfg_.freq, cfg_.bw, cfg_.sf, cfg_.cr,
            RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
            22,   // TX power — irrelevant for RX, but required by begin()
            8,    // preamble symbols
            t.tcxo, t.ldo
        );
        if (state == RADIOLIB_ERR_NONE) break;
        radio_.standby();
    }
    if (state != RADIOLIB_ERR_NONE) {
        return false;
    }

    radio_.setDio2AsRfSwitch(true);
    radio_.setRfSwitchPins(SX126X_RXEN, RADIOLIB_NC);
    radio_.setCRC(1);
    radio_.setCurrentLimit(140.0f);

    radio_.setDio1Action(rx_isr);

    state = radio_.startReceive();
    return state == RADIOLIB_ERR_NONE;
}

RxResult LoRaReceiver::tick() {
    RxResult result = {};  // valid = false by default

    if (!rx_flag) {
        return result;
    }
    rx_flag = false;

    uint8_t buf[PACKET_LEN] = {};
    int16_t state = radio_.readData(buf, PACKET_LEN);

    // Restart receive immediately after reading so no packets are missed.
    radio_.startReceive();

    if (state != RADIOLIB_ERR_NONE) {
        return result;
    }

    // Extract system_id (first 8 bytes, may be null-padded).
    memcpy(result.system_id, buf, SYSID_MAX_LEN);
    result.system_id[SYSID_MAX_LEN] = '\0';

    result.fox_num = buf[8];
    result.rssi    = radio_.getRSSI();
    result.valid   = true;

    return result;
}
