#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <InternalFileSystem.h>

#include "ConfigStore.h"
#include "SerialConfig.h"
#include "LoRaBeacon.h"

// ---------------------------------------------------------------------------
// XIAO nRF52840 + SX1262 pin mapping
// ---------------------------------------------------------------------------
#define LORA_NSS    4   // D4  P0.04
#define LORA_DIO1   1   // D1  P0.03
// RESET is not wired from D2 to the SX1262 NRESET pin on this hardware;
// BUSY going LOW at power-on confirms the chip self-initialises fine.
// Pass RADIOLIB_NC so RadioLib skips the hardware reset sequence.
#define LORA_RESET  RADIOLIB_NC
#define LORA_BUSY   3   // D3  P0.29
#define LORA_RXEN   5   // D5  P0.05  (RX path of RF switch)

// XIAO nRF52840 built-in LEDs (active LOW)
#define LED_ERROR  11   // Red  — fast blink when config is missing
#define LED_TX     12   // Blue — toggled per TX inside LoRaBeacon

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
// LoRa SPI uses SPIM2 (NRF_SPIM2) to avoid nRF52840 Errata #195 (SPIM3).
//
// IMPORTANT — static init order trap:
//   SPIClass constructors look up g_ADigitalPinMap[] at construction time.
//   When a SPIClass is a static/global, its constructor runs before main(),
//   before g_ADigitalPinMap is populated — so all pin lookups return 0.
//   Passing dummy pin 0 avoids the bad lookup; the real pins are set in
//   setup() via setPins() once g_ADigitalPinMap is valid.
static SPIClass loraSPI(NRF_SPIM2, 0, 0, 0);  // pins fixed in setup()

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RESET, LORA_BUSY, loraSPI);

BeaconConfig cfg;
SerialConfig serial_cfg(cfg);
LoRaBeacon   beacon(cfg, radio);

static bool    radio_ready = false;
static int16_t radio_init_err = 0;

// ---------------------------------------------------------------------------
// Radio helpers
// ---------------------------------------------------------------------------
// Attempt radio initialisation.  Mirrors MeshCore's CustomSX1262::std_init():
// try 1.8 V TCXO first (EByte E22 / RAK4630 style), fall back to 0.0 V (crystal
// or externally-powered TCXO) when the first attempt fails.  Each attempt
// drives RESET so the chip starts from a clean state.
static bool radio_begin() {
    // Try four combinations: tcxo 1.8V/0V × LDO/DCDC regulator.
    // On every attempt RadioLib hard-resets the chip (findChip), so each
    // try starts from a clean STDBY_RC state.
    struct { float tcxo; bool ldo; } tries[] = {
        { 1.8f, false },   // TCXO on DIO3, DCDC  (EByte E22, RAK4631 ...)
        { 1.8f, true  },   // TCXO on DIO3, LDO   (unstable VCC boards)
        { 0.0f, false },   // crystal / ext-TCXO, DCDC
        { 0.0f, true  },   // crystal / ext-TCXO, LDO
    };
    for (auto &t : tries) {
        Serial.print("INFO: try tcxo="); Serial.print(t.tcxo);
        Serial.print(" ldo="); Serial.println(t.ldo);
        int16_t state = radio.begin(
            cfg.freq, cfg.bw, cfg.sf, cfg.cr,
            RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
            22, 8, t.tcxo, t.ldo
        );
        if (state == RADIOLIB_ERR_NONE) {
            Serial.print("INFO: radio OK tcxo="); Serial.print(t.tcxo);
            Serial.print(" ldo="); Serial.println(t.ldo);
            radio.setDio2AsRfSwitch(true);
            radio.setRfSwitchPins(LORA_RXEN, RADIOLIB_NC);
            radio.setCRC(1);
            radio.setCurrentLimit(140.0f);
            return true;
        }
        // Print device errors — key bit: 0x0020 = XOSC_START_ERR (TCXO fault)
        radio.standby();
        Serial.print("ERR: begin state="); Serial.print(state);
        Serial.print(" devErr=0x"); Serial.println(radio.getDeviceErrors(), HEX);
    }
    radio_init_err = -2;
    return false;
}

static void radio_apply_params() {
    radio.setFrequency(cfg.freq);
    radio.setBandwidth(cfg.bw);
    radio.setSpreadingFactor(cfg.sf);
    radio.setCodingRate(cfg.cr);
}

// ---------------------------------------------------------------------------
// LED helpers
// ---------------------------------------------------------------------------
static void leds_init() {
    pinMode(LED_ERROR, OUTPUT);
    digitalWrite(LED_ERROR, HIGH);  // off (active low)
    pinMode(LED_TX, OUTPUT);
    digitalWrite(LED_TX, HIGH);     // off (active low)
}

static void blink_error_led() {
    static uint32_t last_toggle = 0;
    static bool     led_on      = false;
    if (millis() - last_toggle >= 250) {
        last_toggle = millis();
        led_on = !led_on;
        digitalWrite(LED_ERROR, led_on ? LOW : HIGH);
    }
}

static void error_led_off() {
    digitalWrite(LED_ERROR, HIGH);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    leds_init();

    // 3-second delay: long enough for the user to open a serial terminal
    // before any diagnostic output is printed.  Blink LED_ERROR during wait
    // so it's obvious the device is booting.
    for (int i = 0; i < 6; i++) {
        digitalWrite(LED_ERROR, (i & 1) ? HIGH : LOW);
        delay(500);
    }
    digitalWrite(LED_ERROR, HIGH);

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
    // INVALID_STATE since the CB is already owned, but the GPIO step still runs
    // and the correct PSEL values are preserved).
    {
        // XIAO nRF52840 SPI hardware pins (P1.13/14/15):
        //   SCK=D8=45=P1.13  MISO=D9=46=P1.14  MOSI=D10=47=P1.15
        static const uint8_t S2_SCK  = 45;
        static const uint8_t S2_MOSI = 47;
        static const uint8_t S2_MISO = 46;

        static const nrfx_spim_t spim2 = NRFX_SPIM_INSTANCE(2);
        nrfx_spim_uninit(&spim2);  // free nrfx CB (no-op if already clear)

        NRF_SPIM2->ENABLE = 0;    // ensure PSEL writes are not ignored
        __DSB();

        nrfx_spim_config_t cfg = {
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
        nrfx_spim_init(&spim2, &cfg, NULL, NULL);
    }
    loraSPI.setPins(PIN_SPI_MISO, PIN_SPI_SCK, PIN_SPI_MOSI);
    loraSPI.begin();  // sets initialized=true; applies high-drive GPIO

    // Wait up to 500 ms for SX1262 BUSY to go LOW (chip ready).
    {
        uint32_t t0 = millis();
        while ((bool)digitalRead(LORA_BUSY) && millis() - t0 < 500) {}
        if ((bool)digitalRead(LORA_BUSY))
            Serial.print("WARN: SX1262 BUSY still HIGH after 500ms\r\n");
    }

    // -----------------------------------------------------------------------
    // I2C — not strictly needed (no RTC) but initialise for future expansion.
    Wire.begin();

    // Persistent storage
    InternalFS.begin();

    // Load config; fall back to factory defaults if nothing is stored.
    config_init_defaults(cfg);
    if (config_load(cfg)) {
        Serial.print("Config loaded from flash.\r\n");
    } else {
        Serial.print("No stored config — using defaults. Set SYSID and FOX, then SAVE.\r\n");
    }

    // Initialise radio hardware (always, regardless of config validity).
    radio_ready = radio_begin();
    if (!radio_ready) {
        // Radio failure — report but do NOT halt; keep serial config accessible
        // so the user can still connect and diagnose via GET ALL / SET commands.
        Serial.print("WARN: radio hardware failure. Serial config still active.\r\n");
    }

    if (config_is_valid(cfg)) {
        Serial.print("Config valid — starting beacon.\r\n");
        beacon.begin();
    } else {
        Serial.print("Config incomplete. Connect and run: GET ALL\r\n");
    }
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    serial_cfg.tick();

    if (serial_cfg.wasConfigSaved()) {
        // Config was just saved.  If the radio is already running just update
        // parameters; otherwise attempt a fresh init (in case it failed at
        // boot before a valid config was present).
        if (radio_ready) {
            radio_apply_params();
        } else {
            radio_ready = radio_begin();
            if (!radio_ready) {
                Serial.print("WARN: radio still offline after SAVE\r\n");
            }
        }
        if (radio_ready && config_is_valid(cfg)) {
            error_led_off();
            beacon.begin();
        }
    }

    if (config_is_valid(cfg)) {
        beacon.tick();
    } else {
        blink_error_led();
    }

    if (!radio_ready) {
        // Periodically remind the user that radio init failed so they can see
        // it even if they connect after boot.
        static uint32_t last_warn = 0;
        if (millis() - last_warn >= 5000) {
            last_warn = millis();
            extern const uint32_t g_ADigitalPinMap[];
            Serial.print("WARN: radio init failed (RadioLib err ");
            Serial.print(radio_init_err);
            Serial.print("). PIN MAP: SCK=");  Serial.print(g_ADigitalPinMap[8]);
            Serial.print(" MISO=");            Serial.print(g_ADigitalPinMap[9]);
            Serial.print(" MOSI=");            Serial.print(g_ADigitalPinMap[10]);
            Serial.print(" NSS=");             Serial.print(g_ADigitalPinMap[4]);
            Serial.print(" RST=");             Serial.print(g_ADigitalPinMap[2]);
            Serial.print(" BUSY=");            Serial.println(g_ADigitalPinMap[3]);
        }
    }
}
