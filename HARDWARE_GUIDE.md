# EchoBand — Hardware & Wiring Guide
**Project:** EchoBand ("One Message, Every Sense")  
**Target:** HACK'TVM 2026 (Deadline: 3:00 PM IST)  
**Audience:** Hardware Assembly Lead  

---

## 1. Pre-Build Sanity & Parts Check

Before stripping wires or mounting anything, verify these items against your desk:

| Item | Required Qty | Status / Crucial Action |
| :--- | :--- | :--- |
| **ESP32 Dev Board** | **2 units** | **CRITICAL:** You need 1 board for the Station and 1 board for the Band. (The Arduino UNO R4 WiFi on the desk is **NOT** used). |
| **SG90 Micro Servos** | **4 units** | Used as haptic tappers ($M1$ to $M4$). If your kit only has 3, **request a 4th servo from the organizers right away**. |
| **L298N Driver Board** | 1 unit | Used **strictly as a 5V regulated power supply**, NOT for driving motors. |
| **MPU6050 Module** | 1 unit | Accelerometer for double-tap acknowledgment ($I^2C$ address `0x68`). |
| **9V Battery & Clip** | 1 unit | Powers the servo rail via the L298N. |
| **Push Buttons** | 5 units | 4 for Station presets, 1 for Band profile selector. |
| **Active Buzzer** | 1 unit | For the Band (Blind profile audio rhythm). |
| **Colored LEDs** | 4 units | Blue (Platform), Yellow/Orange (Delay), Green (Boarding), Red (Urgent). |
| **220 Ω Resistors** | 4 units | Color code: **Red-Red-Brown-Gold**. (Do **NOT** use 220 kΩ: Red-Red-Yellow-Gold). |
| **Parts to Put Away** | — | **Do NOT wire:** Ultrasonic sensors (HC-SR04), Soil sensor, GPS, RFID tags. Store them away to avoid clutter. |

---

## 2. Power Architecture & The Common Ground Rule

> [!CAUTION]
> **#1 Hackathon Failure Mode: Ground Loops & Power Starvation**
> * Servos draw peak surge currents that will brown out or fry an ESP32 if powered from its 3.3V or 5V pin.
> * Never power SG90 servos from ESP32 pins.
> * **ALL grounds must be tied together into a single common ground rail.**

```
[9V Battery]
    │ (+) Red lead
    ▼
[L298N 12V Screw Terminal]
    │
[L298N Board] ── (Ensure 5V-EN Jumper cap is ON!)
    │
    ├─► [+5V Screw Terminal] ──► Breadboard Servo Power Rail (+5V)
    │                                └─► Also to Band ESP32 VIN (optional untethered)
    │
    └─► [GND Screw Terminal] ──► COMMON GROUND RAIL
                                     ├─► 9V Battery (-) Black lead
                                     ├─► Band ESP32 GND
                                     ├─► All 4 Servos GND (Brown/Black wires)
                                     ├─► MPU6050 GND
                                     └─► Buzzer / LED GNDs
```

### L298N Setup as a 5V Regulator:
1. Connect the **9V battery positive (+)** to the L298N screw terminal labeled **`+12V`** (or `VMS`).
2. Connect the **9V battery negative (-)** to the L298N screw terminal labeled **`GND`**.
3. **Check the 5V jumper:** Ensure the two-pin jumper cap behind the screw terminals labeled **`5V-EN`** is in place.
4. Take your servo power from the L298N terminal labeled **`+5V`**. This powers the SG90 servo VCC rail.
5. *Firmware Safety:* Software is hard-coded to fire only **one servo at a time** with a 50 ms stagger to keep current consumption well within the regulator's 0.5 A limit.

---

## 3. Unit A: Station Board (Build First — Fast & Simple)

The Station receives inputs from operator buttons or the phone (via BLE / USB Serial) and broadcasts them over ESP-NOW.

* **Power:** Powered directly from Laptop USB.
* **Buttons (Internal Pull-Up):** Wire one leg of each button to the GPIO pin, and the opposite leg to **GND**.

| Component | ESP32 GPIO | Wiring Note |
| :--- | :--- | :--- |
| **Button 1 (Platform)** | `GPIO 32` | One pin to GPIO 32, other pin to GND |
| **Button 2 (Delay)** | `GPIO 33` | One pin to GPIO 33, other pin to GND |
| **Button 3 (Departure)** | `GPIO 25` | One pin to GPIO 25, other pin to GND |
| **Button 4 (Urgent)** | `GPIO 26` | One pin to GPIO 26, other pin to GND |
| *(Optional) Station Buzzer* | `GPIO 27` | Positive to GPIO 27, Negative to GND |

---

## 4. Unit B: Wearable Band (High Precision Assembly)

### 4.1 SG90 Servo Actuators ($M1, M2, M3, M4$)
* **Wiring:**
  * **Red Wire (VCC):** Connected to the L298N regulated +5V rail.
  * **Brown / Black Wire (GND):** Connected to Common Ground.
  * **Orange / Yellow Wire (Signal):** Connected directly to ESP32:
    * **$M1$:** `GPIO 18`
    * **$M2$:** `GPIO 19`
    * **$M3$:** `GPIO 23`
    * **$M4$:** `GPIO 25`
* **Mechanical Placement:**
  * Mount servos along the armband or wrist mount in a sequential line: $M1 \to M2 \to M3 \to M4$.
  * Attach a single straight plastic arm (horn) to each servo.
  * **Rest Position:** The arm sits slightly back. During a signal, it swings 30° to 40° to gently poke/tap the skin, then returns.
  * **Mechanical Clearance:** Ensure no servo horn is mechanically blocked or jammed against a rigid surface; stalling draws > 800 mA and can damage the motor.

### 4.2 MPU6050 Accelerometer (Double-Tap Recognition)
* **Wiring ($I^2C$):**
  * **VCC:** ESP32 `3.3V` (GY-521 modules have an onboard 3.3V regulator).
  * **GND:** Common Ground.
  * **SDA:** `GPIO 21`
  * **SCL:** `GPIO 22`
  * **AD0:** Must be connected to **GND** (locks $I^2C$ address to `0x68`).
* **Mounting Requirement:**
  * Must be **firmly strapped or taped to the rigid part of the armband**.
  * If the sensor hangs loosely on jumper wires, the double-tap acceleration threshold ($> 2.5\,\text{g}$) will not trigger.

### 4.3 Profile Push Button
* Cycles profiles: Deaf $\to$ Deafblind $\to$ Blind $\to$ Language.
* One leg to **`GPIO 4`**, opposite leg to **GND** (software `INPUT_PULLUP`).

### 4.4 Active Buzzer (Blind Profile)
* Positive pin (+) $\to$ **`GPIO 26`**.
* Negative pin (-) $\to$ **GND**.

### 4.5 Deaf Profile Status LEDs
* Cathode (short leg) to **GND**.
* Anode (long leg) through a **220 Ω resistor** to the GPIO:
  * **Blue LED (Platform):** `GPIO 13`
  * **Orange/Yellow LED (Delay):** `GPIO 14`
  * **Green LED (Departure):** `GPIO 27`
  * **Red LED (Urgent):** `GPIO 33`

---

## 5. Master Band Pinout Quick-Reference

| Pin / Line | Device Connected | Wire Color / Terminal | Description |
| :--- | :--- | :--- | :--- |
| `GPIO 18` | Servo $M1$ | Signal (Orange/Yellow) | Wave lead / Platform prefix tap |
| `GPIO 19` | Servo $M2$ | Signal (Orange/Yellow) | Platform number taps / Delay tap |
| `GPIO 23` | Servo $M3$ | Signal (Orange/Yellow) | Boarding / Departure long tap |
| `GPIO 25` | Servo $M4$ | Signal (Orange/Yellow) | Urgent alert rapid taps |
| `GPIO 21` | MPU6050 SDA | Jumper | $I^2C$ Data |
| `GPIO 22` | MPU6050 SCL | Jumper | $I^2C$ Clock |
| `GPIO 4`  | Profile Button | Jumper to Switch | Mode switch (Deaf / Blind / etc.) |
| `GPIO 26` | Active Buzzer | Buzzer (+) | Acoustic rhythm for blind profile |
| `GPIO 13` | Blue LED | Anode via 220 Ω | Platform visual indicator |
| `GPIO 14` | Orange/Yellow LED | Anode via 220 Ω | Delay visual indicator |
| `GPIO 27` | Green LED | Anode via 220 Ω | Departure visual indicator |
| `GPIO 33` | Red LED | Anode via 220 Ω | Urgent visual indicator |
| `L298N 5V` | All 4 Servos VCC | Servo Red wires | Regulated servo power rail |
| `GND` | All Components | Brown / Black wires | Common reference ground |

---

## 6. Pre-Power-On Checklist (Avoid Magic Smoke)

1. [ ] **Resistor Verification:** Check the LED resistors with a multimeter or color code. Confirm they are 220 Ω (Red-Red-Brown-Gold) and not 220 kΩ.
2. [ ] **No 9V to ESP32:** Confirm the 9V battery leads connect ONLY to the L298N terminal block, never directly to the ESP32.
3. [ ] **L298N Jumper:** Check that the 5V-EN jumper cap is securely plugged in.
4. [ ] **Continuity & Ground Check:** Verify continuity between all ground points (Battery (-), L298N GND, ESP32 GND, Servo GNDs, MPU6050 GND).
5. [ ] **Brownout Mitigation:** Standard 9V alkaline block batteries suffer steep voltage drops under load. Keep a micro-USB cable handy to power the Band ESP32 logic independently while testing servos with the 9V battery.
