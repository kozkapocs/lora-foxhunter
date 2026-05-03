#include "Display.h"
#include <Arduino.h>

// SH1106 I2C address defined in variant.h as DISPLAY_ADDRESS (0x3D).
// PIN_OLED_RESET is defined as -1 (no dedicated reset pin).

#define RSSI_MIN  (-120.0f)  // dBm → 0 signal
#define RSSI_MAX  ( 0.0f)    // dBm → 10 signal
#define BAR_X       0
#define BAR_Y      36        // Progress bar position
#define BAR_W     128
#define BAR_H      18        // Bar height
#define BAR_INNER  (BAR_W - 2)  // 126 usable pixels inside the border
#define TICK_Y     54        // Tick marks start (bottom of bar)
#define TICK_H      3        // Tick mark height
#define LABEL_Y    56        // Labels below tick marks (moved up to fit in 64px)
#define SMILEY_CENTER_X  64
#define SMILEY_CENTER_Y  23
#define SMILEY_RADIUS    10

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

    // Row 0: system identifier + fox number on same line
    oled_.setTextSize(1);
    oled_.setCursor(0, 0);
    oled_.print("ID: ");
    oled_.print(system_id[0] ? system_id : "(not set)");
    oled_.print(" | F: ");
    oled_.print(selected_fox);

    if (has_signal) {
        uint8_t scale = rssi_to_scale(rssi);
        
        // Draw smiley face based on signal strength
        draw_smiley(scale);
        
        // Draw progress bar with fill
        oled_.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, SH110X_WHITE);
        draw_bar(scale);
    } else {
        // No signal: show "---" instead of smiley
        oled_.setTextSize(2);
        oled_.setCursor(50, 20);
        oled_.print("---");
        
        // Empty bar
        oled_.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, SH110X_WHITE);
    }
    
    // Always draw tick marks and labels (ruler-style) at 0, 2, 4, 6, 8, 10
    // Tick positions: 0%=1px, 20%=26px, 40%=51px, 60%=76px, 80%=101px, 100%=126px
    oled_.drawFastVLine(1, TICK_Y, TICK_H, SH110X_WHITE);              // 0
    oled_.drawFastVLine(26, TICK_Y, TICK_H, SH110X_WHITE);             // 2
    oled_.drawFastVLine(51, TICK_Y, TICK_H, SH110X_WHITE);             // 4
    oled_.drawFastVLine(76, TICK_Y, TICK_H, SH110X_WHITE);             // 6
    oled_.drawFastVLine(101, TICK_Y, TICK_H, SH110X_WHITE);            // 8
    oled_.drawFastVLine(126, TICK_Y, TICK_H, SH110X_WHITE);            // 10
    
    // Labels below tick marks
    oled_.setTextSize(1);
    oled_.setCursor(0, LABEL_Y);
    oled_.print("0");
    oled_.setCursor(24, LABEL_Y);
    oled_.print("2");
    oled_.setCursor(49, LABEL_Y);
    oled_.print("4");
    oled_.setCursor(74, LABEL_Y);
    oled_.print("6");
    oled_.setCursor(99, LABEL_Y);
    oled_.print("8");
    oled_.setCursor(115, LABEL_Y);
    oled_.print("10");

    oled_.display();
}

void ReceiverDisplay::show_message(const char *msg) {
    oled_.clearDisplay();
    oled_.setTextSize(1);
    oled_.setCursor(0, 28);
    oled_.print(msg);
    oled_.display();
}

void ReceiverDisplay::draw_bar(uint8_t scale) {
    if (scale > 10) scale = 10;
    int fill_w = (int)((scale / 10.0f) * BAR_INNER);
    if (fill_w > 0) {
        oled_.fillRect(BAR_X + 1, BAR_Y + 1, fill_w, BAR_H - 2, SH110X_WHITE);
    }
}

uint8_t ReceiverDisplay::rssi_to_scale(float rssi) {
    float normalized = (rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    return (uint8_t)(normalized * 10.0f);
}

void ReceiverDisplay::draw_smiley(uint8_t scale) {
    const int16_t cx = SMILEY_CENTER_X;
    const int16_t cy = SMILEY_CENTER_Y;
    const int16_t r = SMILEY_RADIUS;
    
    // Draw face circle
    oled_.drawCircle(cx, cy, r, SH110X_WHITE);
    
    // Draw eyes (two small filled circles)
    const int16_t eye_y = cy - 3;
    const int16_t eye_radius = 1;  // Smaller eyes, less scary!
    oled_.fillCircle(cx - 4, eye_y, eye_radius, SH110X_WHITE);
    oled_.fillCircle(cx + 4, eye_y, eye_radius, SH110X_WHITE);
    
    // Draw mouth based on signal strength
    const int16_t mouth_y = cy + 3;
    
    if (scale <= 2) {
        // Sad face (0-2): frown - szélek lent, közép fent
        oled_.drawLine(cx - 4, mouth_y + 1, cx - 2, mouth_y, SH110X_WHITE);
        oled_.drawLine(cx - 2, mouth_y, cx + 2, mouth_y, SH110X_WHITE);
        oled_.drawLine(cx + 2, mouth_y, cx + 4, mouth_y + 1, SH110X_WHITE);
    } else if (scale <= 5) {
        // Neutral face (3-5): straight line
        oled_.drawLine(cx - 4, mouth_y, cx + 4, mouth_y, SH110X_WHITE);
    } else if (scale <= 8) {
        // Happy face (6-8): smile - szélek fent, közép lent
        oled_.drawLine(cx - 4, mouth_y - 1, cx - 2, mouth_y, SH110X_WHITE);
        oled_.drawLine(cx - 2, mouth_y, cx + 2, mouth_y, SH110X_WHITE);
        oled_.drawLine(cx + 2, mouth_y, cx + 4, mouth_y - 1, SH110X_WHITE);
    } else {
        // Very happy face (9-10): big smile - szélek fent, közép lent
        oled_.drawLine(cx - 5, mouth_y - 1, cx - 3, mouth_y + 1, SH110X_WHITE);
        oled_.drawLine(cx - 3, mouth_y + 1, cx, mouth_y + 2, SH110X_WHITE);
        oled_.drawLine(cx, mouth_y + 2, cx + 3, mouth_y + 1, SH110X_WHITE);
        oled_.drawLine(cx + 3, mouth_y + 1, cx + 5, mouth_y - 1, SH110X_WHITE);
    }
}
