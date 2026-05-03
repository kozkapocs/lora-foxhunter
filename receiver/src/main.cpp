#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <InternalFileSystem.h>
#include <nrfx_spim.h>

#include "ConfigStore.h"
#include "SerialConfig.h"
#include "LoRaReceiver.h"
#include "Display.h"

// ---------------------------------------------------------------------------
// Wio Tracker L1 + SX1262 pin mapping (defined in variants/wio-tracker-l1/variant.h)
// ---------------------------------------------------------------------------
//   P_LORA_NSS  = 4  (D4)    SX126X_RXEN = 5  (D5)
//   P_LORA_DIO_1= 1  (D1)    JOYSTICK_UP = 25 (D25)
//   P_LORA_RESET= 2  (D2)    JOYSTICK_DOWN=26 (D26)
//   P_LORA_BUSY = 3  (D3)

// Signal hold time: if no packet arrives within this window the display
// reverts to "no signal" state.
static const uint32_t SIGNAL_HOLD_MS = 600;

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
// LoRa SPI uses SPIM2 (NRF_SPIM2) to avoid nRF52840 Errata #195 (SPIM3).
// Dummy pins 0 passed here to avoid static-init-order g_ADigitalPinMap lookup;
// real pins are assigned in setup() via setPins() + the nrfx fix block.
static SPIClass loraSPI(NRF_SPIM2, 0, 0, 0);

SX1262 radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, loraSPI);

ReceiverConfig cfg;
SerialConfig   serial_cfg(cfg);
LoRaReceiver   receiver(cfg, radio);
ReceiverDisplay display;

// Joystick state (INPUT_PULLUP → active LOW)
static bool joy_up_prev   = HIGH;
static bool joy_down_prev = HIGH;

// Signal state
static uint8_t   selected_fox  = 1;
static bool      has_signal    = false;
static float     last_rssi     = 0.0f;
static uint32_t  last_rx_ms    = 0;

// Display dirty flag — avoid unnecessary I2C redraws
static bool display_dirty = true;

// ---------------------------------------------------------------------------
// Joystick helpers
// ---------------------------------------------------------------------------
static bool joystick_up_pressed() {
    bool state = digitalRead(JOYSTICK_UP);
    bool edge  = (state == LOW && joy_up_prev == HIGH);
    joy_up_prev = state;
    return edge;
}

static bool joystick_down_pressed() {
    bool state = digitalRead(JOYSTICK_DOWN);
    bool edge  = (state == LOW && joy_down_prev == HIGH);
    joy_down_prev = state;
    return edge;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // Joystick pins
    pinMode(JOYSTICK_UP,   INPUT_PULLUP);
    pinMode(JOYSTICK_DOWN, INPUT_PULLUP);

    // I2C for OLED
    Wire.setPins(PIN_WIRE_SDA, PIN_WIRE_SCL);
    Wire.begin();

    // Display — show startup message immediately
    if (!display.begin()) {
        // If display init fails, continue anyway (development without screen).
        Serial.print("WARN: display init failed\r\n");
    } else {
        display.show_message("FoxHunter RX");
    }

    delay(500);  // USB CDC enumeration

    // -----------------------------------------------------------------------
    // Initialise LoRa SPI on SPIM2
    // -----------------------------------------------------------------------
    // SPIClass::begin() / nrfx_spim_init() does NOT disable the peripheral
    // before writing PSEL registers.  If SPIM2 is left enabled from a previous
    // reset the PSEL writes are silently ignored by the hardware.
    // Fix: assert ENABLE=0 with a DSB barrier, then call nrfx_spim_init
    // directly with hardcoded-correct pin numbers, and finally call
    // loraSPI.begin() so SPIClass sets its `initialized` flag and configures
    // the GPIO pins for high-drive (the internal nrfx call will fail with
    // INVALID_STATE since the CB is already owned, but the GPIO step still
    // runs and the correct PSEL values are preserved).
    {
        // Wio Tracker L1 SPI hardware pins:
        //   SCK=D8=P0.30=30  MISO=D9=P0.03=3  MOSI=D10=P0.28=28
        static const uint8_t S2_SCK  = 30;
        static const uint8_t S2_MOSI = 28;
        static const uint8_t S2_MISO = 3;

        static const nrfx_spim_t spim2 = NRFX_SPIM_INSTANCE(2);
        nrfx_spim_uninit(&spim2);  // free nrfx CB (no-op if already clear)

        NRF_SPIM2->ENABLE = 0;    // ensure PSEL writes are not ignored
        __DSB();

        nrfx_spim_config_t spim_cfg = {
            .sck_pin        = S2_SCK,
            .mosi_pin       = S2_MOSI,
            .miso_pin       = S2_MISO,
            .ss_pin         = NRFX_SPIM_PIN_NOT_USED,
            .ss_active_high = false,
            .irq_priority   = 3,
            .orc            = 0xFF,
            .frequency      = NRF_SPIM_FREQ_4M,
            .mode           = NRF_SPIM_MODE_0,
            .bit_order      = NRF_SPIM_BIT_ORDER_MSB_FIRST,
            .miso_pull      = NRF_GPIO_PIN_NOPULL,
        };
        nrfx_spim_init(&spim2, &spim_cfg, NULL, NULL);
    }
    loraSPI.setPins(PIN_SPI_MISO, PIN_SPI_SCK, PIN_SPI_MOSI);
    loraSPI.begin();  // sets initialized=true; applies high-drive GPIO

    // Wait up to 500 ms for SX1262 BUSY to go LOW (chip ready).
    {
        uint32_t t0 = millis();
        while ((bool)digitalRead(P_LORA_BUSY) && millis() - t0 < 500) {}
        if ((bool)digitalRead(P_LORA_BUSY))
            Serial.print("WARN: SX1262 BUSY still HIGH after 500ms\r\n");
    }

    // Flash filesystem
    InternalFS.begin();

    // Load config
    config_init_defaults(cfg);
    if (config_load(cfg)) {
        Serial.print("Config loaded from flash.\r\n");
    } else {
        Serial.print("No stored config — using defaults. Set SYSID, then SAVE.\r\n");
    }

    // Initialise radio
    if (!receiver.begin()) {
        Serial.print("FATAL: radio init failed. Check hardware.\r\n");
        display.show_message("RADIO ERROR");
        while (true) {
            delay(500);
        }
    }

    Serial.print("Receiver running. Joystick UP/DOWN to select fox (1-255).\r\n");
    display_dirty = true;
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    // --- Serial configuration ---
    serial_cfg.tick();

    if (serial_cfg.wasConfigSaved()) {
        // Re-initialise radio with new parameters
        if (!receiver.begin()) {
            Serial.print("ERR: radio re-init failed after config save\r\n");
        }
        display_dirty = true;
    }

    // --- Joystick: change selected fox ---
    if (joystick_up_pressed()) {
        if (selected_fox < 255) {
            selected_fox++;
        } else {
            selected_fox = 1;
        }
        has_signal    = false;  // reset signal when switching
        display_dirty = true;
    }

    if (joystick_down_pressed()) {
        if (selected_fox > 1) {
            selected_fox--;
        } else {
            selected_fox = 255;
        }
        has_signal    = false;
        display_dirty = true;
    }

    // --- LoRa receive ---
    RxResult pkt = receiver.tick();
    if (pkt.valid) {
        // Filter by system_id
        if (strncmp(pkt.system_id, cfg.system_id, SYSID_MAX_LEN) != 0) {
            // Wrong system — discard silently
        } else if (pkt.fox_num != selected_fox) {
            // Right system, wrong fox — discard silently
        } else {
            // Valid packet for the selected fox
            Serial.print("RX fox=");
            Serial.print(pkt.fox_num);
            Serial.print(" rssi=");
            Serial.print(pkt.rssi);
            Serial.print(" dBm\r\n");

            last_rssi  = pkt.rssi;
            last_rx_ms = millis();
            has_signal = true;
            display_dirty = true;
        }
    }

    // --- Signal hold timeout ---
    if (has_signal && (millis() - last_rx_ms >= SIGNAL_HOLD_MS)) {
        has_signal    = false;
        display_dirty = true;
    }

    // --- Refresh display when needed ---
    if (display_dirty) {
        display.update(cfg.system_id, selected_fox, has_signal, last_rssi);
        display_dirty = false;
    }
}
