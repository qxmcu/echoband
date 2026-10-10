# EchoBand — Complete Wiring and Pinout Reference

This document details all physical connections, power routing, and pin mappings for the EchoBand Station and Wearable Band units.

---

## 1. Power Distribution

```
[9V Battery]
    │ (+) Red lead
    ▼
[L298N 12V Screw Terminal]
    │
[L298N Board] ── (Ensure 5V-EN Jumper cap is ON!)
    │
    ├─► [+5V Screw Terminal] ──► Breadboard Servo Power Rail (+5V)
    │                                └─► ESP32 VIN (optional untethered)
    │
    └─► [GND Screw Terminal] ──► COMMON GROUND RAIL
                                     ├─► 9V Battery (-) Black lead
                                     ├─► Band ESP32 GND
                                     ├─► All 4 Servos GND (Brown/Black wires)
                                     ├─► MPU6050 GND
                                     └─► Buzzer / LED GNDs
```

### Safety & Regulator Rules
* **L298N as 5V Regulator:** 9V battery (+) connects to L298N `+12V` terminal; (-) connects to L298N `GND`. Ensure the **5V-EN** jumper is in place.
* The L298N onboard regulator supplies ~0.5 A. Firmwares are built to activate only **one servo at a time** (50 ms stagger).
* **Common Ground:** Ground of battery, L298N, ESP32, servos, and MPU6050 MUST be bonded together.
* **Logic Levels:** ESP32 logic is 3.3V. Servo signal wires connect directly to GPIO pins. Servos receive 5V power from the L298N rail.

---

## 2. Station Unit Pinout

Powered via Laptop USB.

| Function | ESP32 GPIO | Mode | Description |
| :--- | :--- | :--- | :--- |
| **Platform Preset Button** | `GPIO 32` | `INPUT_PULLUP` | Connect to GND on press (Preset: Platform 2) |
| **Delay Preset Button** | `GPIO 33` | `INPUT_PULLUP` | Connect to GND on press |
| **Departure Preset Button** | `GPIO 25` | `INPUT_PULLUP` | Connect to GND on press |
| **Urgent Preset Button** | `GPIO 26` | `INPUT_PULLUP` | Connect to GND on press |
| **Optional Station Buzzer** | `GPIO 27` | `OUTPUT` | Station acoustic confirmation |

---

## 3. Wearable Band Pinout

Powered by L298N 5V rail / USB.

| Function | ESP32 GPIO | Pin Type | Notes |
| :--- | :--- | :--- | :--- |
| **Servo M1 (Attention / Platform)** | `GPIO 18` | PWM / Output | Signal (Orange/Yellow). Power from 5V rail. |
| **Servo M2 (Platform Count / Delay)** | `GPIO 19` | PWM / Output | Signal (Orange/Yellow). Power from 5V rail. |
| **Servo M3 (Departure)** | `GPIO 23` | PWM / Output | Signal (Orange/Yellow). Power from 5V rail. |
| **Servo M4 (Urgent / Cancel)** | `GPIO 25` | PWM / Output | Signal (Orange/Yellow). Power from 5V rail. |
| **MPU6050 SDA** | `GPIO 21` | I2C Data | Connect to module SDA |
| **MPU6050 SCL** | `GPIO 22` | I2C Clock | Connect to module SCL |
| **MPU6050 AD0** | `GND` | Ground | Forces I2C address to `0x68` |
| **Profile Cycle Button** | `GPIO 4` | `INPUT_PULLUP` | Connect to GND on press |
| **Active Buzzer (Blind Profile)** | `GPIO 26` | `OUTPUT` | Active buzzer (+) to GPIO 26, (-) to GND |
| **Blue LED (Platform)** | `GPIO 13` | `OUTPUT` | Anode via 220 Ω resistor, Cathode to GND |
| **Orange/Yellow LED (Delay)** | `GPIO 14` | `OUTPUT` | Anode via 220 Ω resistor, Cathode to GND |
| **Green LED (Departure)** | `GPIO 27` | `OUTPUT` | Anode via 220 Ω resistor, Cathode to GND |
| **Red LED (Urgent)** | `GPIO 33` | `OUTPUT` | Anode via 220 Ω resistor, Cathode to GND |

---

## 4. Mechanical & Sensor Setup

1. **SG90 Servos:**
   - Mount in line along the band: $M1 \to M2 \to M3 \to M4$.
   - Horn at rest sits slightly retracted; during tap, swings 30° to 40° against user's wrist/arm.
   - Do not allow horns to physically jam.
2. **MPU6050:**
   - Mount firmly to the band strap so double-taps on the arm/wrist directly transmit shock waves to the accelerometer.
