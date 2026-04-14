#pragma once

#include <Adafruit_SH110X.h>
#include <stdint.h>

// Drives the 1.3" SH1106 OLED (128×64 px, I2C address 0x3D).
//
// Screen layout:
//   Row 0  (y= 0, size 1): system identifier
//   Row 1  (y=10, size 1): selected fox number
//   Row 2  (y=22, size 2): RSSI value in dBm, or "---" when no signal
//   Bar    (y=52,  h=10):  horizontal progress bar proportional to RSSI
//
// RSSI bar mapping: -120 dBm → empty, -30 dBm → full.
class ReceiverDisplay {
public:
    ReceiverDisplay();

    // Initialise the display. Call once in setup() after Wire.begin().
    // Returns true on success.
    bool begin();

    // Render the current state onto the screen.
    // Call whenever the displayed information changes.
    void update(const char *system_id, uint8_t selected_fox,
                bool has_signal, float rssi);

    // Show a single centred status message (used during startup / error).
    void show_message(const char *msg);

private:
    Adafruit_SH1106G oled_;

    void draw_bar(float rssi);
};
