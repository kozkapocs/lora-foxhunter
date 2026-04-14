#include "Display.h"
#include <Arduino.h>

// SH1106 I2C address defined in variant.h as DISPLAY_ADDRESS (0x3D).
// PIN_OLED_RESET is defined as -1 (no dedicated reset pin).

#define RSSI_MIN  (-120.0f)  // dBm → empty bar
#define RSSI_MAX  ( -30.0f)  // dBm → full bar
#define BAR_X       0
#define BAR_Y      52
#define BAR_W     128
#define BAR_H      10
#define BAR_INNER  (BAR_W - 2)  // 126 usable pixels inside the border

ReceiverDisplay::ReceiverDisplay()
    : oled_(128, 64, &Wire, PIN_OLED_RESET) {}

bool ReceiverDisplay::begin() {
    if (!oled_.begin(DISPLAY_ADDRESS, true)) {
        return false;
    }
    oled_.setTextColor(SH110X_WHITE);
    oled_.clearDisplay();
    oled_.display();
    return true;
}

void ReceiverDisplay::update(const char *system_id, uint8_t selected_fox,
                              bool has_signal, float rssi) {
    oled_.clearDisplay();

    // Row 0: system identifier
    oled_.setTextSize(1);
    oled_.setCursor(0, 0);
    oled_.print("SYS: ");
    oled_.print(system_id[0] ? system_id : "(not set)");

    // Row 1: selected fox number
    oled_.setCursor(0, 10);
    oled_.print("FOX: ");
    oled_.print(selected_fox);

    // Row 2: RSSI value (large text)
    oled_.setTextSize(2);
    oled_.setCursor(0, 22);
    if (has_signal) {
        oled_.print((int)rssi);
        oled_.print(" dBm");
    } else {
        oled_.print("  ---");
    }

    // Progress bar
    oled_.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, SH110X_WHITE);
    if (has_signal) {
        draw_bar(rssi);
    }

    oled_.display();
}

void ReceiverDisplay::show_message(const char *msg) {
    oled_.clearDisplay();
    oled_.setTextSize(1);
    oled_.setCursor(0, 28);
    oled_.print(msg);
    oled_.display();
}

void ReceiverDisplay::draw_bar(float rssi) {
    float normalized = (rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    int fill_w = (int)(normalized * BAR_INNER);
    if (fill_w > 0) {
        oled_.fillRect(BAR_X + 1, BAR_Y + 1, fill_w, BAR_H - 2, SH110X_WHITE);
    }
}
