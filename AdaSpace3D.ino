/*
 * AdaSpace3D - Ultra-Configurable Firmware
 * Features: Physics Engine (Smoothing, Gamma), 6-Axis Independence, Button Mapping
 */

#include "Adafruit_TinyUSB.h"
#include "TLx493D_inc.hpp"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_seesaw.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "UserConfig.h"

// --- CONSTANTS ---
#ifndef USB_VID
  #define USB_VID 0x256f // Default 3DConnexion
#endif
#ifndef USB_PID
  #define USB_PID 0xc631
#endif
#define HANG_THRESHOLD 50
Adafruit_NeoPixel strip(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// --- CONFIGURATION ---
SpaceMouseConfig config;

// --- HARDWARE GLOBALS ---
const uint8_t physPins[] = {BUTTON1_PIN, BUTTON2_PIN, BUTTON3_PIN, BUTTON4_PIN};
bool currentButtonState[] = {false, false, false, false};
bool prevButtonState[] = {false, false, false, false};

// --- PHYSICS STATE ---
struct PhysicsState {
    double x = 0, y = 0, z = 0; // Filtered values
} physState;

struct MagCalibration {
  double x_neutral = 0.0, y_neutral = 0.0, z_neutral = 0.0;
  bool calibrated = false;
} magCal;

struct SensorWatchdog {
  double last_x = 0.0, last_y = 0.0, last_z = 0.0;
  int sameValueCount = 0;
  unsigned long lastResetTime = 0;
} watchdog;

using namespace ifx::tlx493d;
TLx493D_A1B6 magCable(Wire1, TLx493D_IIC_ADDR_A0_e);
TLx493D_A1B6 magSolder(Wire, TLx493D_IIC_ADDR_A0_e);
TLx493D_A1B6* activeSensor = nullptr;
bool streaming = false;

Adafruit_seesaw encoder(&Wire1);
int32_t encoder_position = 0;

// HID Report Descriptor (Standard 3DConnexion)
static const uint8_t spaceMouse_hid_report_desc[] = {
  0x05, 0x01, 0x09, 0x08, 0xA1, 0x01, 0xA1, 0x00, 0x85, 0x01, 0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F, 0x36, 0x00, 0x80, 0x46, 0xFF, 0x7F,
  0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x75, 0x10, 0x95, 0x03, 0x81, 0x02, 0xC0,
  0xA1, 0x00, 0x85, 0x02, 0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F, 0x36, 0x00, 0x80, 0x46, 0xFF, 0x7F,
  0x09, 0x33, 0x09, 0x34, 0x09, 0x35, 0x75, 0x10, 0x95, 0x03, 0x81, 0x02, 0xC0,
  0xA1, 0x00, 0x85, 0x03, 0x05, 0x09, 0x19, 0x01, 0x29, 0x20, 0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x20, 0x81, 0x02, 0xC0,
  0xC0
};

Adafruit_USBD_HID usb_hid;
#define CALIBRATION_SAMPLES 50

// --- HELPER FUNCTIONS ---

void loadConfig() {
  EEPROM.begin(512);
  EEPROM.get(0, config);
  
  if (config.magic != CONFIG_MAGIC) {
    config.magic = CONFIG_MAGIC;
    config.smoothing = DEFAULT_SMOOTHING;
    config.gamma = DEFAULT_GAMMA;
    config.deadzone = DEFAULT_DEADZONE;
    
    config.gain_tx = DEFAULT_GAIN_TRANS; config.gain_ty = DEFAULT_GAIN_TRANS; config.gain_tz = DEFAULT_GAIN_ZOOM;
    config.gain_rx = DEFAULT_GAIN_ROT;   config.gain_ry = DEFAULT_GAIN_ROT;   config.gain_rz = DEFAULT_GAIN_ROT; // Unused for magnet, meant for encoder
    
    config.inv_tx = 0; config.inv_ty = 0; config.inv_tz = 0;
    config.inv_rx = 0; config.inv_ry = 0; config.inv_rz = 0;
    
    config.swap_xy = false;
    
    config.button_map[0] = 1; config.button_map[1] = 2; // Left/Right Click
    config.button_map[2] = 27; config.button_map[3] = 28; // Standard Macro
    
    config.led_mode = DEFAULT_LED_MODE;
    config.led_color_r = DEFAULT_LED_R;
    config.led_color_g = DEFAULT_LED_G;
    config.led_color_b = DEFAULT_LED_B;
    config.led_brightness = DEFAULT_LED_BRIGHTNESS;
    
    config.enc_enabled = true;
    config.enc_gain = 5.0f;
    config.enc_invert = false;
  }
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

void updateHardwareLeds(uint8_t r, uint8_t g, uint8_t b) {
  int brightness = (r * 77 + g * 150 + b * 29) >> 8; 
  brightness = (brightness * config.led_brightness) / 255;
  analogWrite(PIN_SIMPLE, brightness);
  
  // Also update NeoPixel
  strip.setBrightness(config.led_brightness);
  strip.fill(strip.Color(r,g,b));
  strip.show();
}

void handleLeds(double totalMove) {
  if (config.led_mode == 0) { // STATIC
    updateHardwareLeds(config.led_color_r, config.led_color_g, config.led_color_b);
  }
  else if (config.led_mode == 1) { // BREATHING
    float val = (exp(sin(millis()/2000.0*PI)) - 0.36787944)*108.0;
    uint8_t r = (config.led_color_r * (int)val) / 255;
    uint8_t g = (config.led_color_g * (int)val) / 255;
    uint8_t b = (config.led_color_b * (int)val) / 255;
    updateHardwareLeds(r, g, b);
  }
  else if (config.led_mode == 2) { // REACTIVE
      int minScale = 50;  
      int maxScale = 255; 
      int currentScale = minScale;

      if (totalMove > config.deadzone) {
         int addedIntensity = (int)(totalMove * 30.0);
         currentScale = constrain(minScale + addedIntensity, minScale, maxScale);
      }
      
      uint8_t r = (config.led_color_r * currentScale) / 255;
      uint8_t g = (config.led_color_g * currentScale) / 255;
      uint8_t b = (config.led_color_b * currentScale) / 255;
      updateHardwareLeds(r, g, b);
  }
}

void handleSerialConfig() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "?") {
      JsonDocument doc;
      doc["smooth"] = config.smoothing;
      doc["gamma"] = config.gamma;
      doc["dz"] = config.deadzone;
      
      // Gains
      doc["g_tx"] = config.gain_tx; doc["g_ty"] = config.gain_ty; doc["g_tz"] = config.gain_tz;
      doc["g_rx"] = config.gain_rx; doc["g_ry"] = config.gain_ry; doc["g_rz"] = config.gain_rz;
      
      // Inverts
      doc["i_tx"] = config.inv_tx; doc["i_ty"] = config.inv_ty; doc["i_tz"] = config.inv_tz;
      doc["i_rx"] = config.inv_rx; doc["i_ry"] = config.inv_ry; doc["i_rz"] = config.inv_rz;
      
      doc["sw_xy"] = config.swap_xy;
      
      // Buttons
      JsonArray btns = doc["btns"].to<JsonArray>();
      btns.add(config.button_map[0]); btns.add(config.button_map[1]); 
      btns.add(config.button_map[2]); btns.add(config.button_map[3]);
      
      doc["l_mo"] = config.led_mode;
      doc["l_r"] = config.led_color_r; doc["l_g"] = config.led_color_g; doc["l_b"] = config.led_color_b;
      doc["l_br"] = config.led_brightness;
      
      doc["enc"] = config.enc_enabled;
      doc["e_g"] = config.enc_gain;
      doc["e_i"] = config.enc_invert;
      
      serializeJson(doc, Serial);
      Serial.println();
    }
    else if (cmd.startsWith("SAVE:")) {
       String json = cmd.substring(5);
       JsonDocument doc;
       DeserializationError error = deserializeJson(doc, json);
       if (!error) {
           if(!doc["smooth"].isNull()) config.smoothing = doc["smooth"];
           if(!doc["gamma"].isNull()) config.gamma = doc["gamma"];
           if(!doc["dz"].isNull()) config.deadzone = doc["dz"];
           
           if(!doc["g_tx"].isNull()) config.gain_tx = doc["g_tx"];
           if(!doc["g_ty"].isNull()) config.gain_ty = doc["g_ty"];
           if(!doc["g_tz"].isNull()) config.gain_tz = doc["g_tz"];
           if(!doc["g_rx"].isNull()) config.gain_rx = doc["g_rx"];
           if(!doc["g_ry"].isNull()) config.gain_ry = doc["g_ry"];
           if(!doc["g_rz"].isNull()) config.gain_rz = doc["g_rz"];
           
           if(!doc["i_tx"].isNull()) config.inv_tx = doc["i_tx"];
           if(!doc["i_ty"].isNull()) config.inv_ty = doc["i_ty"];
           if(!doc["i_tz"].isNull()) config.inv_tz = doc["i_tz"];
           if(!doc["i_rx"].isNull()) config.inv_rx = doc["i_rx"];
           if(!doc["i_ry"].isNull()) config.inv_ry = doc["i_ry"];
           if(!doc["i_rz"].isNull()) config.inv_rz = doc["i_rz"];
           
           if(!doc["sw_xy"].isNull()) config.swap_xy = doc["sw_xy"];
           
           JsonArray btns = doc["btns"];
           if(!btns.isNull()) {
             config.button_map[0] = btns[0]; config.button_map[1] = btns[1];
             config.button_map[2] = btns[2]; config.button_map[3] = btns[3];
           }

           if(!doc["l_mo"].isNull()) config.led_mode = doc["l_mo"];
           if(!doc["l_r"].isNull()) config.led_color_r = doc["l_r"];
           if(!doc["l_g"].isNull()) config.led_color_g = doc["l_g"];
           if(!doc["l_b"].isNull()) config.led_color_b = doc["l_b"];
           if(!doc["l_br"].isNull()) config.led_brightness = doc["l_br"];
           
           if(!doc["enc"].isNull()) config.enc_enabled = doc["enc"];
           if(!doc["e_g"].isNull()) config.enc_gain = doc["e_g"];
           if(!doc["e_i"].isNull()) config.enc_invert = doc["e_i"];
           
           saveConfig();
           Serial.println("OK");
       } else {
           Serial.println("ERR_JSON");
       }
    }
    else if (cmd == "STREAM:ON") { streaming = true; Serial.println("OK_STREAM"); }
    else if (cmd == "STREAM:OFF") { streaming = false; Serial.println("OK_STREAM_OFF"); }
    else if (cmd == "RESET") {
       config.magic = 0;
       loadConfig();
       saveConfig();
       Serial.println("OK_RESET");
    }
  }
}

void setup() {
  loadConfig();
  
  pinMode(PIN_SIMPLE, OUTPUT);
  strip.begin();
  handleLeds(0); // Init

  Serial.begin(115200);

  for(uint8_t i = 0; i < 4; i++) pinMode(physPins[i], INPUT_PULLUP);

  TinyUSBDevice.setID(USB_VID, USB_PID);
  usb_hid.setReportDescriptor(spaceMouse_hid_report_desc, sizeof(spaceMouse_hid_report_desc));
  usb_hid.setPollInterval(2);
  usb_hid.begin();

  while(!TinyUSBDevice.mounted()) delay(100);
  
  pinMode(MAG_POWER_PIN, OUTPUT);
  digitalWrite(MAG_POWER_PIN, HIGH);
  delay(10); 

  Wire1.begin(); Wire1.setClock(400000);
  if (magCable.begin()) {
     activeSensor = &magCable;
     updateHardwareLeds(0, 255, 0); delay(500); 
  } 
  else {
     Wire.begin(); Wire.setClock(400000);
     if (magSolder.begin()) {
        activeSensor = &magSolder;
        updateHardwareLeds(0, 255, 255); delay(500);
     } else {
         // ERROR
         while(1) { updateHardwareLeds(255,0,0); delay(100); updateHardwareLeds(0,0,0); delay(100); }
     }
  }

  // Encoder
  if(config.enc_enabled) {
      if(encoder.begin(0x36)) {
        encoder_position = encoder.getEncoderPosition();
        encoder.setGPIOInterrupts((uint32_t)1 << 24, true);
        encoder.enableEncoderInterrupt();
      }
  }

  // Calibrate
  double sumX = 0, sumY = 0, sumZ = 0;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    double x, y, z;
    if(activeSensor->getMagneticField(&x, &y, &z)) {
      sumX += x; sumY += y; sumZ += z;
    }
    delay(10);
  }
  magCal.x_neutral = sumX / CALIBRATION_SAMPLES;
  magCal.y_neutral = sumY / CALIBRATION_SAMPLES;
  magCal.z_neutral = sumZ / CALIBRATION_SAMPLES;
  magCal.calibrated = true;
}

double applyPhysics(double val, double gamma) {
    if(abs(val) < 0.001) return 0;
    return (val > 0 ? 1 : -1) * pow(abs(val), gamma);
}

void loop() {
  handleSerialConfig();

  // Button Logic
  for(uint8_t i = 0; i < 4; i++) {
    currentButtonState[i] = (digitalRead(physPins[i]) == LOW);
    if (currentButtonState[i] != prevButtonState[i]) {
      // Use mapped HID ID
      uint8_t hidID = config.button_map[i];
      if(hidID > 0) setButtonStateHID(hidID, currentButtonState[i]);
      prevButtonState[i] = currentButtonState[i];
    }
  }

  // Sensor Logic
  if (activeSensor) {
      double x, y, z;
      if(activeSensor->getMagneticField(&x, &y, &z)) {
          // 1. Remove bias
          x -= magCal.x_neutral;
          y -= magCal.y_neutral;
          z -= magCal.z_neutral;

          // 2. Hardware Orientation (Swap XY)
          if(config.swap_xy) {
              double temp = x; x = y; y = temp;
          }

          // 3. Smoothing (EMA)
          // physState retains history. 
          // alpha = 1.0 - smoothing. 
          // If smoothing is 0.9, alpha is 0.1 (slow). If smoothing is 0.0, alpha is 1.0 (instant).
          double alpha = 1.0 - constrain(config.smoothing, 0.0, 0.99);
          physState.x = (x * alpha) + (physState.x * (1.0 - alpha));
          physState.y = (y * alpha) + (physState.y * (1.0 - alpha));
          physState.z = (z * alpha) + (physState.z * (1.0 - alpha));
          
          x = physState.x; y = physState.y; z = physState.z;
          
          double totalMove = abs(x) + abs(y) + abs(z);
          // handleLeds(totalMove); // LEDS DISABLED

          // 4. Deadzone
          if(abs(x) < config.deadzone) x = 0;
          if(abs(y) < config.deadzone) y = 0;
          if(abs(z) < config.deadzone) z = 0; // Using global DZ for Z too

          // 5. Gamma & Gain & Invert Calculation
          // Note: Standard SpaceMouse axes:
          // TX, TY: Trans (Pan)
          // TZ: Zoom
          // RX, RY: Tilt
          // RZ: Twist (Encoder or Mag)
          
          // --- TX (Pan L/R) ---
          double val_tx = applyPhysics(x, config.gamma) * config.gain_tx;
          if(config.inv_tx) val_tx = -val_tx;
          
          // --- TY (Pan U/D) ---
          double val_ty = applyPhysics(y, config.gamma) * config.gain_ty;
          if(config.inv_ty) val_ty = -val_ty;
          
          // --- TZ (Zoom) ---
          double val_tz = applyPhysics(z, config.gamma) * config.gain_tz;
          if(config.inv_tz) val_tz = -val_tz;
          
          // --- RX (Tilt F/B) ---
          // Reuse Y axis raw data for tilt
          double val_rx = applyPhysics(y, config.gamma) * config.gain_rx;
          if(config.inv_rx) val_rx = -val_rx;

          // --- RY (Tilt L/R) ---
          // Reuse X axis raw data for tilt
          double val_ry = applyPhysics(x, config.gamma) * config.gain_ry;
          if(config.inv_ry) val_ry = -val_ry;

          // --- RZ (Twist) ---
          // From Encoder if enabled
          int16_t rz = 0;
          if(config.enc_enabled) {
             int32_t new_pos = encoder.getEncoderPosition();
             int32_t diff = new_pos - encoder_position;
             encoder_position = new_pos;
             
             if(config.enc_invert) diff = -diff;
             rz = (int16_t)(diff * config.enc_gain * 10.0); 
          }

          // Invert mapping for final output
          // Note: Mag outputs -x, -y usually for proper mouse feel
          send_tx_rx_reports((int16_t)(-val_tx), (int16_t)(-val_ty), (int16_t)(val_tz), 
                             (int16_t)(val_rx), (int16_t)(val_ry), rz);
          
          if(streaming) {
             static unsigned long lastStream = 0;
             if(millis() - lastStream > 50) { // 20Hz Limit
               lastStream = millis();
               Serial.print("GRA:");
               Serial.print(x); Serial.print(",");
               Serial.print(y); Serial.print(",");
               Serial.print(z); Serial.print(",");
               Serial.print(currentButtonState[0]); Serial.print(",");
               Serial.print(currentButtonState[1]); Serial.print(",");
               Serial.print(currentButtonState[2]); Serial.print(",");
               Serial.println(currentButtonState[3]);
             }
          }
      }
  }
  delay(2);
}

void setButtonStateHID(uint8_t hidButton, bool pressed) {
  if (!TinyUSBDevice.mounted()) return;
  uint8_t report[4] = {0, 0, 0, 0};
  if (pressed && hidButton <= 32) {
    report[(hidButton - 1) / 8] = (1 << ((hidButton - 1) % 8));
  }
  if (usb_hid.ready()) usb_hid.sendReport(3, report, 4);
}

void send_tx_rx_reports(int16_t tx, int16_t ty, int16_t tz, int16_t rx, int16_t ry, int16_t rz) {
  if (!TinyUSBDevice.mounted() || !usb_hid.ready()) return;

  uint8_t tx_report[6] = {(uint8_t)tx, (uint8_t)(tx>>8), (uint8_t)ty, (uint8_t)(ty>>8), (uint8_t)tz, (uint8_t)(tz>>8)};
  usb_hid.sendReport(1, tx_report, 6);
  
  delayMicroseconds(500);
  
  uint8_t rx_report[6] = {(uint8_t)rx, (uint8_t)(rx>>8), (uint8_t)ry, (uint8_t)(ry>>8), (uint8_t)rz, (uint8_t)(rz>>8)};
  usb_hid.sendReport(2, rx_report, 6);
}
