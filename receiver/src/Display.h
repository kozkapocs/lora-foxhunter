#pragma once

#include <Adafruit_SH110X.h>
#include <stdint.h>

// Drives the 1.3" SH1106 OLED (128×64 px, I2C address 0x3D).
//
// Screen layout:
//   Row 0  (y= 0, size 1): ID: XXXXXXXX | F: NNN (system ID + fox number)
//   Smiley (y=12-35):      visual signal strength indicator (sad → happy)
//   Bar    (y=36-54, h=18): horizontal progress bar (0-10 scale)
//   Ticks  (y=54-57, h=3):  ruler-style tick marks at 0, 2, 4, 6, 8, 10
//   Labels (y=56, size 1):  numeric labels below ticks (0, 2, 4, 6, 8, 10)
//
// RSSI mapping: -120 dBm → 0, 0 dBm → 10.
// Smiley states: 0-2 sad, 3-5 neutral, 6-8 happy, 9-10 very happy.
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

    // Convert RSSI (dBm) to 0-10 scale.
    uint8_t rssi_to_scale(float rssi);

    // Draw a smiley face based on signal strength (0-10).
    // 0-2: sad, 3-5: neutral, 6-8: happy, 9-10: very happy.
    void draw_smiley(uint8_t scale);

    // Draw the progress bar based on signal scale (0-10).
    void draw_bar(uint8_t scale);
};
