#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include <Arduino.h>

// --- PIN DEFINITIONS ---
#define PIN_SDA             0
#define PIN_SCL             1
#define PIN_NEOPIXEL        12 // QT Py RP2040 Built-in
#define MAG_POWER_PIN       11 // Power pin for sensor

// Simple LED (optional)
#define PIN_SIMPLE          25 

// Buttons
#define BUTTON1_PIN         29
#define BUTTON2_PIN         28
#define BUTTON3_PIN         27
#define BUTTON4_PIN         26

// --- CONFIGURATION STRUCT ---
// Increment MAGIC to force EEPROM reset when structure changes
#define CONFIG_MAGIC 0xAD45DAC4 

struct SpaceMouseConfig {
  uint32_t magic;

  // --- PHYSICS ---
  float smoothing;        // 0.0 (raw) to 0.95 (super smooth)
  float gamma;            // 1.0 (linear) to 3.0 (cubic precision)
  float deadzone;         // Global Deadzone (approx 0.5 - 5.0)

  // --- AXIS GAINS (Sensitivity) ---
  float gain_tx, gain_ty, gain_tz;
  float gain_rx, gain_ry, gain_rz;

  // --- AXIS INVERTS ---
  bool inv_tx, inv_ty, inv_tz;
  bool inv_rx, inv_ry, inv_rz;

  // --- MOUNTING ---
  bool swap_xy;           // Rotates Virtual North by 90deg

  // --- BUTTON MAPPING ---
  uint8_t button_map[4];  // HID Button IDs for Phys 1-4

  // --- LEDS ---
  uint8_t led_mode;       // 0=Static, 1=Breathing, 2=Reactive
  uint8_t led_color_r;
  uint8_t led_color_g;
  uint8_t led_color_b;
  uint8_t led_brightness; // 0-255

  // --- ENCODER ---
  bool enc_enabled;
  float enc_gain;
  bool enc_invert;
};

// Global Config Instance
extern SpaceMouseConfig config;

// Default Values
#define DEFAULT_SMOOTHING 0.5f
#define DEFAULT_GAMMA     1.5f
#define DEFAULT_DEADZONE  1.0f

#define DEFAULT_GAIN_TRANS 100.0f
#define DEFAULT_GAIN_ROT   40.0f
#define DEFAULT_GAIN_ZOOM  50.0f

#define DEFAULT_LED_MODE 2
#define DEFAULT_LED_R    0
#define DEFAULT_LED_G    255
#define DEFAULT_LED_B    255
#define DEFAULT_LED_BRIGHTNESS 50

#endif
