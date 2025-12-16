# AdaSpace3D - The Ultimate RP2040 SpaceMouse Firmware 🚀

**AdaSpace3D** is a high-performance, open-source firmware for DIY SpaceMice based on the RP2040 (Adafruit QT Py, Raspberry Pi Pico, etc.). It features a **Physics Engine** for smooth movement and a **Real-Time Web Configurator**.

---

## ✨ Features

### 🔌 **One-Click Setup**
- **No Coding Skills Required**: Just run `RUN_ME.bat`.
- **Auto-Installer**: Automatically downloads Node.js, installs dependencies, and launches the dashboard.

### 🎛️ **Web Configurator**
- **Real-Time Tuning**: Adjust Sensitivity, Deadzones, and Colors instantly without rebooting.
- **Physics Engine**: 
  - **Smoothing**: Adjustable low-pass filter for buttery smooth motion.
  - **Gamma Curve**: non-linear response for precision at low speeds.
  - **Circular Deadzone**: Advanced radial deadzone calculation.
- **System Log**: Build-in terminal to monitor TX/RX commands and errors.

### 🛠️ **Hardware Support**
- **Sensors**: Compatible with `TLx493D` 3D magnetic sensors.
- **Encoders (Experimental)**: Optional I2C Encoder support (toggleable).
- **MCU**: Designed for RP2040 (Native USB Support).

---

## 🚀 Quick Start

1. **Download** this repository.
2. Double-click **`RUN_ME.bat`** (Windows).
    - It will install everything needed.
    - It will open the **Configurator** in your browser.
3. Plug in your device.
4. Click **Connect** in the dashboard.
5. **Calibrate**: Leave the knob alone and click "Reset Zero".

---

## ⚙️ Hardware Configuration

If you are building your own hardware, you can customize the pinout and options in `UserConfig.h` before flashing.

**Common Settings:**

```cpp
// UserConfig.h

// Encoders
// Enable 'I2C Connection' in the dashboard if you use a Seesaw Encoder.
```

---

## 🔧 Dashboard Controls

| Panel | Function |
| :--- | :--- |
| **Live View** | Shows raw joystick output (Green/Blue/Red bars). |
| **Actions** | Save Config, Reset to Defaults, **Build & Flash Firmware**. |
| **Movement** | Tuning for Smoothness, Gamma, and Deadzone. |
| **Axes** | Individual Gain (Speed) and Invert per axis (Tx, Ty, Tz, Rx, Ry, Rz). |
| **System Log** | Detailed connection and error status. |

---

## 🏗️ How to Flash (Update Firmware)

No need to install Arduino IDE! 

1. Open the **Configurator** (`RUN_ME.bat`).
2. Make your changes in the UI.
3. Click **🔨 Build & Flash Firmware**.
4. The server will compile the code and automatically copy it to your device.

---

## 📜 License
MIT License. Open Source Hardware & Software.
