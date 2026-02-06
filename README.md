# 🥁 DIY Bluetooth E-Drum Module (ESP32 + I2S)

A professional-grade DIY electronic drum module built on the **ESP32** platform. This project features dual-mode operation: low-latency **BLE MIDI** for triggering VSTs and an **I2S Analog Output** for direct monitoring through a 1/4" mono jack.

## 🚀 Key Features
*   **Wireless MIDI:** Low-latency Bluetooth Low Energy (BLE) connectivity.
*   **High-Fidelity Audio:** Uses I2S protocol for clean analog signal generation via an external DAC.
*   **Dynamic Sensitivity:** Intelligent peak detection for velocity-sensitive drum hits.
*   **Mono Jack Output:** Ready to plug into any standard guitar amp or PA system.

---

## 🛠️ Components List
| Component | Purpose | Source/Link |
| :--- | :--- | :--- |
| **ESP32 DevKit V1** | Main Microcontroller | [ESP32 on Amazon](https://www.amazon.com) |
| **PCM5102 DAC** | I2S to Analog Conversion | [PCM5102 Guide](https://learn.adafruit.com) |
| **Piezo Elements** | Drum Pad Sensors (27mm-35mm) | [Piezo Sensors](https://www.sparkfun.com) |
| **Mono Jack (1/4")** | Physical Audio Output | [6.35mm Jacks](https://www.mouser.com) |
| **1M Ohm Resistor** | Voltage protection for ADC | - |

---

## 🔌 Wiring Schematic

### 1. I2S DAC (Audio Output)
Connect the PCM5102 DAC to the ESP32 as follows:
*   **VCC** -> 3.3V
*   **GND** -> GND
*   **LRCK** -> GPIO 25
*   **BCK** -> GPIO 26
*   **DIN** -> GPIO 22

### 2. Piezo Triggers
*   **Piezo (+)** -> GPIO 32 (Analog Pin)
*   **Piezo (-)** -> GND
*   *Note: Place a 1M Ohm resistor in parallel with the Piezo to prevent voltage spikes from damaging the ESP32.*

---

## 💻 Software Configuration

### Required Libraries
Ensure you have the following libraries installed in your [Arduino IDE](https://www.arduino.cc):
1.  **ESP32-BLE-MIDI** (by lathoub) - For Bluetooth MIDI transmission.
2.  **ESP32 I2S** (Built-in) - For the analog audio signal.

### Quick Start
1. Clone this repository.
2. Open `e_drum_main.ino` in Arduino IDE.
3. Select **DOIT ESP32 DEVKIT V1** as your board.
4. Upload and pair your PC/Tablet to "ESP32-EDrum-Module" via Bluetooth.

---

<!-- ## 📂 Project Structure
```text
├── src/
│   ├── e_drum_main.ino    # Main logic & MIDI handling
│   ├── i2s_audio.h        # I2S configuration & synthesis
│   └── config.h           # Thresholds and Pin definitions
├── hardware/
│   └── schematic.png      # Circuit diagram
└── README.md              # Project documentation -->