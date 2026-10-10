# EchoBand — Hardware Assembly, Flashing & Testing Field Manual
**Project:** EchoBand ("One Message, Every Sense")  
**Target:** HACK'TVM 2026 (Deadline: 3:00 PM IST)  
**System Architecture:** Single ESP32 Armband Unit (Zero Infrastructure)  
**Audience:** Hardware & Assembly Lead  

---

## Table of Contents
1. [Important Architecture Note (No Station Needed!)](#1-important-architecture-note)
2. [Parts Check & Visual Identification](#2-parts-check--visual-identification)
3. [Power Supply & L298N 5V Regulator Setup](#3-power-supply--l298n-5v-regulator-setup)
4. [Step-by-Step Breadboard Wiring Walkthrough](#4-step-by-step-breadboard-wiring-walkthrough)
5. [Master GPIO Pinout Quick-Reference Table](#5-master-gpio-pinout-quick-reference-table)
6. [Mechanical Armband Assembly](#6-mechanical-armband-assembly)
7. [How to Flash the Firmware (Arduino IDE Guide)](#7-how-to-flash-the-firmware-arduino-ide-guide)
8. [System Health Status LEDs & Fallback Protections](#8-system-health-status-leds--fallback-protections)
9. [Step-by-Step Bench Testing & Verification](#9-step-by-step-bench-testing--verification)
10. [Troubleshooting & Emergency Fixes](#10-troubleshooting--emergency-fixes)

---

## 1. Important Architecture Note

> [!IMPORTANT]
> **We are building ONLY ONE board: The Wearable Armband.**  
> * Transport stations will never install custom hardware for us.
> * The **wearer's own smartphone** does all the listening, Whisper speech AI, and UI display.
> * The **ESP32 on the armband** receives 3-byte commands over Bluetooth Low Energy (BLE), triggers the tactile waves, and detects the wearer's wrist double-tap.
> * **Put away the second ESP32, ultrasonic sensors, soil sensors, and Arduino UNO board.** You only need 1 ESP32.

---

## 2. Parts Check & Visual Identification

Before plugging anything in, inspect your components:

| Part | Quantity | Identification Tips |
| :--- | :---: | :--- |
| **ESP32 Dev Board** | **1** | NodeMCU / DevKit 30-pin or 38-pin board. Has a micro-USB / Type-C port. |
| **SG90 Micro Servos** | **4** | Tiny blue 9g servos labeled $M1, M2, M3, M4$.<br>• **Brown wire** = Ground (GND)<br>• **Red wire** = 5V Power (VCC)<br>• **Orange/Yellow wire** = PWM Signal |
| **L298N Driver Board** | **1** | Red module with large aluminum heat sink. Used **strictly as a 5V regulator**, NOT for driving motors. |
| **MPU6050 Accelerometer** | **1** | Small blue GY-521 board with 8 pins: `VCC, GND, SCL, SDA, XDA, XCL, AD0, INT`. |
| **Active Buzzer** | **1** | Black circular cylinder with sticker. Longer leg = `+`, Shorter leg = `-`. |
| **5mm Colored LEDs** | **4** | 1x Blue (Platform), 1x Orange (Delay), 1x Green (Departure), 1x Red (Urgent).<br>• **Long leg** = Anode (`+`)<br>• **Short leg / flat rim** = Cathode (`-`) |
| **220 Ω Resistors** | **4** | **Color bands: RED — RED — BROWN — GOLD**.<br>⚠️ Check carefully! Do NOT use 220 kΩ (Red-Red-Yellow-Gold) or LEDs will stay dark. |
| **Push Button** | **1** | 4-pin tactile momentary switch for cycling profile modes. |
| **9V Battery & Clip** | **1** | Standard 9V battery with snap clip (Red = `+`, Black = `-`). |
| **Breadboard & Jumpers** | **1 kit**| Male-to-Male (M-M) and Male-to-Female (M-F) jumper wires. |

---

## 3. Power Supply & L298N 5V Regulator Setup

> [!CAUTION]
> **NEVER Power Servos from ESP32 Pins!**  
> An ESP32's onboard regulator can only output ~250 mA. One moving SG90 servo draws up to 500–800 mA. Powering servos directly from the ESP32 will cause brownout reboot loops or fry the board.  
> All servo power comes from the **L298N 5V regulator rail**.

```
                   [9V Battery]
                 (+) Red    (-) Black
                    │          │
                    ▼          ▼
             ┌───────────────────────────┐
             │   [L298N Driver Board]    │
             │                           │
             │  [+12V]   [GND]   [+5V]   │  <-- 3-pin Screw Terminal
             └───┬─────────┬───────┬─────┘
                 │         │       │
                 │         │       └─────► BREADBOARD RED (+) RAIL (5V for Servos & VIN)
                 │         │
                 │         └─────────────► BREADBOARD BLUE (-) RAIL (COMMON GROUND)
                 │                                │
                 │                 ┌──────────────┼──────────────┐
                 │                 ▼              ▼              ▼
                 │            ESP32 GND      Servos GND     MPU6050 GND
                 │
           (Battery +)
```

### Exact Terminal Screws on the L298N:
Look at the green 3-terminal screw block on the edge of the L298N board:
1. **Left Terminal (`+12V` or `VMS`):** Insert the **Red (+) lead** of the 9V battery clip. Tighten screw.
2. **Center Terminal (`GND`):** 
   * Insert the **Black (-) lead** of the 9V battery clip.
   * **ALSO** insert a jumper wire that runs to the **Breadboard Blue (-) rail**.
   * Tighten screw securely.
3. **Right Terminal (`+5V`):** Insert a jumper wire that runs to the **Breadboard Red (+) rail**. Tighten screw.
4. **The `5V-EN` Jumper Cap:** Look right behind the 3 screw terminals. There is a 2-pin header with a black jumper cap labeled **5V-EN**. **LEAVE THIS JUMPER ON**. This enables the onboard 78M05 chip to step 9V down to clean 5V.

---

## 4. Step-by-Step Breadboard Wiring Walkthrough

Follow these steps one by one:

### Step 1: Establish Common Ground
* Connect the jumper from L298N `GND` to the **Breadboard Blue (-) rail**.
* Connect one of the ESP32 `GND` pins to the **Breadboard Blue (-) rail**.
* Connect the jumper from L298N `+5V` to the **Breadboard Red (+) rail**.
* *(All ground references are now unified. If grounds are not unified, servos will twitch wildly and I2C will fail).*

---

### Step 2: Wire the 4 SG90 Servos ($M1, M2, M3, M4$)
Each servo has a female 3-pin plug. Plug 3 Male-to-Male jumper wires into each:

1. **All Red wires** $\to$ **Breadboard Red (+) Rail** (L298N 5V power).
2. **All Brown wires** $\to$ **Breadboard Blue (-) Rail** (Common ground).
3. **Orange (Signal) wires** connect to ESP32:
   * **Servo $M1$ (Attention / Platform):** Connect to **`GPIO 18`**
   * **Servo $M2$ (Platform Count / Delay):** Connect to **`GPIO 19`**
   * **Servo $M3$ (Boarding / Departure):** Connect to **`GPIO 23`**
   * **Servo $M4$ (Urgent / Cancel):** Connect to **`GPIO 25`**

> [!NOTE]
> *Software Protection:* The firmware is hard-coded to energize **only ONE servo at a time** with a 50 ms delay between sequential taps. It will never pull more than 0.5 A.

---

### Step 3: Wire the MPU6050 Accelerometer
Connect the GY-521 board to the ESP32:

1. **`VCC`** $\to$ Connect to ESP32 **`3.3V`** pin.
2. **`GND`** $\to$ Connect to **Breadboard Blue (-) Rail**.
3. **`SDA`** $\to$ Connect to ESP32 **`GPIO 21`** ($I^2C$ Data).
4. **`SCL`** $\to$ Connect to ESP32 **`GPIO 22`** ($I^2C$ Clock).
5. **`AD0`** $\to$ Connect to **Breadboard Blue (-) Rail** (**CRITICAL:** Forces $I^2C$ address to `0x68`).
6. *Pins `INT, XDA, XCL` are left unconnected.*

---

### Step 4: Wire the Profile Cycle Button
1. Place the push button across the center divider of the breadboard.
2. Connect **Pin 1** of the button to ESP32 **`GPIO 4`**.
3. Connect **Pin 2** (diagonal from Pin 1) to **Breadboard Blue (-) Rail**.
4. *(No resistor needed: ESP32 uses internal software `INPUT_PULLUP`).*

---

### Step 5: Wire the Active Buzzer (Blind Profile)
1. **Positive (`+` / longer leg)** $\to$ Connect to ESP32 **`GPIO 26`**.
2. **Negative (`-` / shorter leg)** $\to$ Connect to **Breadboard Blue (-) Rail**.

---

### Step 6: Wire the 4 Status LEDs (Deaf Profile)
For each LED, insert a 220 Ω resistor between the ESP32 pin and the LED anode:

```
ESP32 GPIO Pin ──► [220 Ω Resistor] ──► LED Anode (Long Leg) ──► LED Cathode (Short Leg) ──► GND Rail
```

* **Blue LED (Platform Alert):** Resistor to **`GPIO 13`**
* **Orange / Yellow LED (Delay Alert):** Resistor to **`GPIO 14`**
* **Green LED (Departure Alert):** Resistor to **`GPIO 27`**
* **Red LED (Urgent / Cancel Alert):** Resistor to **`GPIO 33`**

---

## 5. Master GPIO Pinout Quick-Reference Table

| ESP32 Pin | Connected Hardware | Wire Color / Connection | Function |
| :---: | :--- | :--- | :--- |
| **`GPIO 18`** | Servo $M1$ | Orange Signal wire | Wave leader / Platform prefix tap |
| **`GPIO 19`** | Servo $M2$ | Orange Signal wire | Platform count taps / Delay alert |
| **`GPIO 23`** | Servo $M3$ | Orange Signal wire | Boarding / Departure long tap |
| **`GPIO 25`** | Servo $M4$ | Orange Signal wire | Urgent rapid taps / Cancel tap |
| **`GPIO 21`** | MPU6050 SDA | Jumper | $I^2C$ Data |
| **`GPIO 22`** | MPU6050 SCL | Jumper | $I^2C$ Clock |
| **`GPIO 4`**  | Profile Switch | Button Pin 1 $\to$ GND | Cycles Deaf $\to$ Deafblind $\to$ Blind $\to$ Language |
| **`GPIO 26`** | Active Buzzer | Buzzer (+) | Acoustic rhythm for blind profile |
| **`GPIO 13`** | Blue LED | Anode via 220 Ω | Platform visual indicator |
| **`GPIO 14`** | Orange LED | Anode via 220 Ω | Delay visual indicator |
| **`GPIO 27`** | Green LED | Anode via 220 Ω | Departure visual indicator |
| **`GPIO 33`** | Red LED | Anode via 220 Ω | Urgent visual indicator |
| **`3.3V`**    | MPU6050 VCC | Jumper | Clean 3.3V sensor logic |
| **`GND`**     | Common GND Rail | Jumper | System universal ground |
| **`VIN`**     | L298N +5V Rail | Jumper | Untethered board power |

---

## 6. Mechanical Armband Assembly

```
         Wrist Strap / Cardboard Armband Mount
  +-------------------------------------------------+
  |   [M1]       [M2]       [M3]       [M4]         |  <-- 4 Servos in line
  |    │          │          │          │           |
  |   ( )        ( )        ( )        ( )          |  <-- Single arm horn on each
  |                                                 |
  |                [ MPU6050 ]                      |  <-- Taped FIRMLY to chassis
  +-------------------------------------------------+
```

1. **Mounting Servos:**
   * Attach servos in a line along a wrist strap or cardboard band: $M1 \to M2 \to M3 \to M4$.
   * Space them roughly 2.5 cm apart.
2. **Horn Orientation:**
   * Install a single plastic arm (horn) on each servo gear.
   * **Rest angle:** Arm points sideways / retracted away from the wrist.
   * **Active tap angle:** Servo swings 35° inward so the tip taps the skin, then immediately returns.
   * **Clearance:** Make sure the horn is not physically jammed against the frame.
3. **Rigid MPU6050 Mounting (CRUCIAL):**
   * The MPU6050 **must be firmly taped or strapped to the rigid body of the armband**.
   * If it dangles loosely on jumper wires, double-taps on the arm will NOT register!

---

## 7. How to Flash the Firmware (Arduino IDE Guide)

### Step 7.1: Install ESP32 Board Support in Arduino IDE
1. Open **Arduino IDE** (v1.8.x or v2.x).
2. Go to **File $\to$ Preferences**.
3. In **Additional boards manager URLs**, paste this URL:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**.
5. Go to **Tools $\to$ Board $\to$ Boards Manager...**
6. Search for `esp32` and click **Install** (by Espressif Systems).

---

### Step 7.2: Install Required Libraries
Go to **Tools $\to$ Manage Libraries...** and install:
1. **`ESP32Servo`** by Kevin Harrington (version 1.1.0 or latest).
2. **`NimBLE-Arduino`** by h2zero (version 1.4.x or latest).
*(Built-in libraries `Wire` and `Preferences` are already included with the ESP32 board package).*

---

### Step 7.3: Open and Upload the Sketch
1. Download or git pull the repository:
   ```text
   https://github.com/qxmcu/echoband.git
   ```
2. In Arduino IDE, go to **File $\to$ Open...** and select:
   ```text
   echoband/band/band.ino
   ```
3. Configure the board settings under **Tools**:
   * **Board:** `ESP32 Dev Module`
   * **Upload Speed:** `921600` (or `115200` if upload fails)
   * **CPU Frequency:** `240MHz`
   * **Flash Frequency:** `80MHz`
   * **Port:** Select the COM port corresponding to your ESP32 (e.g., `COM3`, `COM5`).
4. Click the **Upload** button (Right arrow icon).

> [!TIP]
> **If the upload console gets stuck at:**  
> `Connecting........_____........_____.....`  
> Press and **HOLD the `BOOT` button** on your ESP32 until the console shows `Writing at 0x00010000... (10 %)`, then release it.

---

## 8. System Health Status LEDs & Fallback Protections

During a live demo in front of hackathon judges, loose jumper wires or Bluetooth pairing timeouts can happen. EchoBand includes multi-state health monitoring and 3 layers of failsafe fallbacks:

### 8.1 Multi-State Health Indicator (LED Diagnostics)
When no alert is actively firing, the band's LEDs function as an automatic hardware health monitor:

| LED Indication | System State | What It Means | Action Required |
| :--- | :--- | :--- | :--- |
| **Solid GREEN + Solid Onboard LED** | **HEALTHY & PAIRED** | BLE connected to phone, MPU6050 I2C responding. | Ready for full demo. |
| **Slow Blinking ORANGE (1 Hz)** | **DEGRADED / WAITING** | BLE is waiting for connection, OR MPU6050 is offline. | Connect phone in Chrome, or check MPU wiring. |
| **Solid RED / Rapid Blink** | **CRITICAL FAULT** | Both BLE stack and MPU6050 are offline. | Check power rails and reset ESP32. |

*(Note: When an alert is received, the LEDs switch immediately to showing the alert color: Blue = Platform, Orange = Delay, Green = Departure, Red = Urgent).*

### 8.2 Three-Tier Failsafe Fallbacks
If anything goes wrong during judging, you have 3 separate fallbacks so the demo never fails:

1. **Hardware Fallback (Physical Button ACK):**
   * *Problem:* The MPU6050 accelerometer gets bumped, wire loosens, or the judge doesn't tap hard enough to trigger 2.5g.
   * *Fallback:* Press the physical **Profile Button (`GPIO 4`)** on the band! While an alert is repeating, pressing this button **instantly confirms the alert**, stops the repetition, sends `ACK` to the phone, and blinks confirmation.
2. **Software Fallback (Phone Screen Remote Dismiss):**
   * *Problem:* The band is across the table, or the user cannot reach the button.
   * *Fallback:* As soon as an alert is broadcast, a green button appears on the phone screen: **`⚡ Dismiss / Confirm on Phone (Double-Tap Fallback)`**. Tapping it remotely sends `ALERT_STOP (0)` to the band, marks the timeline `ACKNOWLEDGED`, and resets the UI.
3. **Sensor Auto-Degradation:**
   * *Problem:* MPU6050 completely disconnected.
   * *Fallback:* The firmware automatically marks `mpuHealthy = false`, disables accelerometer polling (preventing I2C bus hangs), switches the status LED to Orange blink, and routes all confirmations through the physical button.

---

## 9. Step-by-Step Bench Testing & Verification

Once uploaded, open the **Serial Monitor** in Arduino IDE at **115200 baud**.

### Test 1: Power-On Twitch Test
* When the board powers on, Servo $M1$ will give **one quick confirmation tap** (100 ms).
* Serial Monitor will print:
  ```text
  === EchoBand — Wearable Haptic Band ===
  [INIT] Active Profile: DEAF (0)
  [OK] MPU6050 Accelerometer ready (+/- 8g)
  [OK] BLE Advertising as 'EchoBand'
  ```

### Test 2: Serial Command Test (No Phone Needed!)
You can test the entire alert and haptic sequence directly through the Serial Monitor input box:
1. Type `A 1 2` and press Enter:
   * **Attention wave fires:** $M1 \to M2 \to M3 \to M4$ swing sequentially.
   * **Platform 2 fires:** $M1$ gives a long tap, then $M2$ short taps twice.
   * **Message ended wave fires:** $M4 \to M3 \to M2 \to M1$ reverse wave.
   * Blue LED illuminates during the alert.
2. Type `A 2 0` and press Enter $\to$ Delay pattern ($M2$ taps twice, Orange LED).
3. Type `A 3 0` and press Enter $\to$ Departure pattern ($M3$ long tap, Green LED).
4. Type `P` and press Enter $\to$ Cycles profile mode. Green LED blinks to confirm.

### Test 3: Double-Tap Acknowledgment Test
1. Type `A 4 0` and press Enter (Urgent alert).
2. Servo $M4$ will rapidly tap 3 times and Red LED will flash. It repeats every 5 seconds.
3. Firmly double-tap with your fingers on the MPU6050 sensor or wrist band.
4. Serial Monitor will print:
   ```text
   [TAP] Double-Tap confirmed! (dt=220 ms, mag=3.10g)
   [STATUS NOTIFY] ACK 100
   ```
5. The urgent repeat immediately stops and $M1$ gives a short confirmation tap!

### Test 4: Web Bluetooth Phone Test
1. Open the Phone Web App in **Chrome on Android** via HTTPS (or laptop Chrome using the Simulator/Serial).
2. Tap **Connect** $\to$ Select **`EchoBand`** in the Bluetooth popup.
3. Tap **Platform Alert** on the phone screen.
4. The band plays the tactile wave, and the phone shows:
   * **1. SENT** $\to$ **2. RECEIVED (Band)**
5. Double-tap the armband $\to$ phone updates instantly to:
   * **3. DOUBLE-TAP (ACK) — Acknowledged!**

---

## 10. Troubleshooting & Emergency Fixes

| Symptom | Probable Cause | Instant Fix |
| :--- | :--- | :--- |
| **Servos twitch or buzz violently** | Floating ground | Firmly push the L298N GND, ESP32 GND, and Servo brown wires into the same breadboard blue rail. |
| **ESP32 resets when servo taps** | 9V battery voltage drop | Standard 9V block batteries drop voltage under heavy load. **Keep the laptop USB cable plugged into the ESP32** for logic power while the 9V battery powers the servos. |
| **MPU6050 says `not detected`** | AD0 pin floating or wrong I2C pins | Ensure `SDA` is in `GPIO 21`, `SCL` is in `GPIO 22`, and **`AD0` is connected to GND**. |
| **LEDs do not light up** | Reversed polarity or 220k resistor | Swap the LED legs (longer leg must go to resistor/GPIO). Check resistor color code: must be Red-Red-Brown (220 Ω), NOT Red-Red-Yellow (220 kΩ). |
| **Double-tap doesn't trigger** | Sensor is loose | Tape the MPU6050 tightly against the rigid structure of the wrist mount so taps produce sharp G-force spikes. |
| **Arduino IDE fails to upload** | ESP32 bootloader waiting | Hold down the physical `BOOT` button on the ESP32 when the console displays `Connecting......`. |
