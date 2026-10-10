# EchoBand — Complete Wiring and Pinout Reference (Single ESP32 Board)

This document details all physical connections, power routing, and pin mappings for the EchoBand Wearable Armband unit.

---

## 1. Power Distribution Schematic

```
       [9V Battery]
       (+) Red wire ──► L298N "+12V" Screw Terminal
       (-) Black wire ─► L298N "GND" Screw Terminal
                              │
                    [L298N Regulator Board]
                    (5V-EN Jumper MUST BE ON!)
                              │
          ┌───────────────────┴───────────────────┐
          ▼                                       ▼
  L298N "+5V" Terminal                    L298N "GND" Terminal
          │                                       │
          ▼                                       ▼
BREADBOARD RED RAIL (+5V)               BREADBOARD BLUE RAIL (GND)
(Powers all 4 Servos & ESP32 VIN)       (COMMON REFERENCE GROUND)
                                                  │
                                 ┌────────────────┼────────────────┐
                                 ▼                ▼                ▼
                            ESP32 GND        Servos GND       MPU6050 GND
```

### Safety & Power Rules:
* **Never Power Servos from ESP32 Pins:** Servos draw peak currents up to 800 mA. They MUST be powered by the L298N +5V rail.
* **Common Ground:** The battery (-), L298N GND, ESP32 GND, Servos GND, and MPU6050 GND must all be connected to a single common breadboard ground rail.
* **Logic Levels:** ESP32 3.3V logic drives the servo signal lines directly (SG90 accepts 3.3V PWM).

---

## 2. Master Wearable Band Pinout Table

| ESP32 Pin | Connected Component | Wire / Terminal | Purpose / Notes |
| :---: | :--- | :--- | :--- |
| **`GPIO 18`** | Servo $M1$ | Signal (Orange) | Wave leader / Platform marker tap |
| **`GPIO 19`** | Servo $M2$ | Signal (Orange) | Platform count taps / Delay alert taps |
| **`GPIO 23`** | Servo $M3$ | Signal (Orange) | Boarding / Departure long tap |
| **`GPIO 25`** | Servo $M4$ | Signal (Orange) | Urgent rapid taps / Cancel tap |
| **`GPIO 21`** | MPU6050 SDA | Jumper | $I^2C$ Data line |
| **`GPIO 22`** | MPU6050 SCL | Jumper | $I^2C$ Clock line |
| **`GPIO 4`**  | Profile Button | Jumper to GND | Internal `INPUT_PULLUP`, cycles profile |
| **`GPIO 26`** | Active Buzzer | Buzzer (+) | Acoustic rhythm for blind profile |
| **`GPIO 13`** | Blue LED | Anode via 220 Ω | Platform visual indicator |
| **`GPIO 14`** | Orange/Yellow LED | Anode via 220 Ω | Delay visual indicator |
| **`GPIO 27`** | Green LED | Anode via 220 Ω | Departure visual indicator |
| **`GPIO 33`** | Red LED | Anode via 220 Ω | Urgent visual indicator |
| **`3.3V`**    | MPU6050 VCC | Jumper | Sensor logic power |
| **`GND`**     | Common Ground Rail | Jumper | Universal circuit ground |
| **`VIN`**     | L298N +5V Rail | Jumper | Untethered board power |

---

## 3. Resistor & LED Identification

* **LEDs:** Long leg = Anode (+), Short leg / flat rim edge = Cathode (-).
* **220 Ω Resistor Color Code:** **Red — Red — Brown — Gold**.
* Do NOT use 220 kΩ (Red — Red — Yellow — Gold).
