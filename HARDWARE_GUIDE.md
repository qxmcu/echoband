# EchoBand — The Complete Hardware & Assembly Master Guide
**Project:** EchoBand ("One Message, Every Sense")  
**Target:** HACK'TVM 2026 (Deadline: 3:00 PM IST)  
**System Architecture:** **Single-Board (ONE ESP32 Band, Zero Station Infrastructure)**  

---

## 0. The Pivot: What Changed?
* **Zero Infrastructure:** There is **NO station board**, **NO second ESP32**, and **NO ESP-NOW**. 
* The **wearer's smartphone** handles all speech listening, display, and AI.
* Your job is to build **ONE perfect wearable armband unit** containing **1 ESP32**, 4 servos, MPU6050, buzzer, LEDs, and the L298N regulator.

---

## 1. Visual Parts Identification & Pre-Check

Before picking up a screwdriver or wire stripper, verify each component:

### 1.1 Parts You MUST Have on Your Desk:
1. **ESP32 Dev Board (Qty: 1):** 30-pin or 38-pin ESP32 NodeMCU/DevKit.
2. **SG90 Micro Servos (Qty: 4):**
   * Three wires per servo: **Brown** = Ground (GND), **Red** = 5V Power (VCC), **Orange/Yellow** = PWM Signal.
   * 4 single straight plastic horns (arms) and mounting screws.
3. **L298N Motor Driver Board (Qty: 1):** Used strictly as a **5V voltage regulator**, NOT to drive motors.
4. **MPU6050 Accelerometer / Gyroscope (Qty: 1):** Blue GY-521 breakout board with 8 pins (VCC, GND, SCL, SDA, XDA, XCL, AD0, INT).
5. **Active Buzzer (Qty: 1):** Has a sticker on top and two pins (Longer pin = `+`, Shorter pin = `-`).
6. **Colored 5mm LEDs (Qty: 4):**
   * 1x Blue (Platform)
   * 1x Orange/Yellow (Delay)
   * 1x Green (Departure)
   * 1x Red (Urgent / Cancel)
   * *Pin identification:* **Long leg = Anode (+)**, **Short leg / flat rim edge = Cathode (-)**.
7. **220 Ω Resistors (Qty: 4):**
   * **Color Bands:** **RED — RED — BROWN — GOLD** (220 Ω).
   * ⚠️ **DO NOT USE 220 kΩ:** RED — RED — YELLOW — GOLD (LEDs will not light up!).
8. **Tactile Push Button (Qty: 1):** For the profile switcher.
9. **9V Battery & Battery Clip (Qty: 1):** Heavy-duty 9V block battery.
10. **Breadboard & Jumper Wires:** Male-to-Male (M-M) and Male-to-Female (M-F) jumpers.

### 1.2 Parts to Put Away (Do NOT use):
* Ultrasonic sensors (HC-SR04), Soil sensor, Arduino Uno R4 WiFi, GPS, RFID modules. Put them back in the kit box so they don't cause confusion.

---

## 2. Power Architecture: The L298N 5V Regulator Setup

> [!CAUTION]
> **#1 Rule of Hardware at Hackathons: NEVER Power Servos from ESP32 Pins!**
> The ESP32's onboard regulator can only output ~250 mA. An SG90 servo draws up to 500–800 mA during movement. If you power servos from the ESP32, the board will brown out, crash, or burn out its silicon.

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

### Exact Terminal Block Connections on the L298N:
The L298N has a green 3-terminal screw block on one side:
1. **Left Terminal (`+12V` or `VMS`):** Insert the **Red (+) lead** of your 9V battery clip. Tighten the screw.
2. **Middle Terminal (`GND`):** 
   * Insert the **Black (-) lead** of your 9V battery clip.
   * ALSO insert a jumper wire here that runs to your **Breadboard Ground (Blue) Rail**.
   * Tighten the screw firmly.
3. **Right Terminal (`+5V`):** Insert a jumper wire that runs to your **Breadboard Power (Red) Rail**. Tighten the screw.
4. **The 5V Enable Jumper:** Look directly behind the 3 screw terminals. There is a 2-pin header with a black jumper cap labeled **5V-EN**. **LEAVE THIS JUMPER ON**. This enables the onboard 78M05 voltage regulator to step down 9V to a stable 5V.

---

## 3. Step-by-Step Breadboard Wiring Walkthrough

Follow these steps in exact sequential order:

```
+--------------------------------------------------------------------------+
|  RED POWER RAIL   (+5V from L298N)  ==> All Servo Red Wires, ESP32 VIN   |
|  BLUE GROUND RAIL (COMMON GND)      ==> L298N GND, ESP32 GND, All GNDs   |
+--------------------------------------------------------------------------+
```

### Step 1: Establish the Common Ground & Power Rails
1. Connect the **L298N `+5V`** wire to the **Red (+) rail** of the breadboard.
2. Connect the **L298N `GND`** wire to the **Blue (-) rail** of the breadboard.
3. Connect one of the **ESP32 GND pins** to the **Blue (-) rail**.
4. *(Optional Untethered Power):* Connect the **ESP32 `VIN` (or `5V`) pin** to the **Red (+) rail**.  
   *Note: While developing at your laptop, leave ESP32 plugged into USB. The common ground between USB and L298N MUST still be connected!*

---

### Step 2: Wire the 4 SG90 Servos ($M1, M2, M3, M4$)
Each servo has a 3-pin female connector (Brown, Red, Orange). Use Male-to-Male jumper wires:

| Servo | Purpose / Signal | Red Wire (Power) | Brown Wire (Ground) | Orange Wire (Signal) |
| :--- | :--- | :--- | :--- | :--- |
| **Servo $M1$** | Attention / Platform | Breadboard Red Rail (+5V) | Breadboard Blue Rail (GND) | **ESP32 `GPIO 18`** |
| **Servo $M2$** | Platform Count / Delay | Breadboard Red Rail (+5V) | Breadboard Blue Rail (GND) | **ESP32 `GPIO 19`** |
| **Servo $M3$** | Boarding / Departure | Breadboard Red Rail (+5V) | Breadboard Blue Rail (GND) | **ESP32 `GPIO 23`** |
| **Servo $M4$** | Urgent / Cancel | Breadboard Red Rail (+5V) | Breadboard Blue Rail (GND) | **ESP32 `GPIO 25`** |

> [!NOTE]
> *Software Safety:* The firmware is hard-coded to energize **only ONE servo at a time** with a 50 ms delay between taps. This prevents current spikes from tripping the L298N regulator.

---

### Step 3: Wire the MPU6050 Accelerometer
The MPU6050 communicates with the ESP32 via $I^2C$:

| MPU6050 Pin | ESP32 Connection | Why? |
| :--- | :--- | :--- |
| **VCC** | ESP32 **`3.3V`** pin | Power (GY-521 modules operate cleanly at 3.3V) |
| **GND** | Breadboard **Blue Rail (GND)** | Common ground |
| **SCL** | ESP32 **`GPIO 22`** | $I^2C$ Clock line |
| **SDA** | ESP32 **`GPIO 21`** | $I^2C$ Data line |
| **AD0** | Breadboard **Blue Rail (GND)** | **CRITICAL:** Pulling AD0 to GND locks the $I^2C$ address to `0x68` |
| *INT / XDA / XCL*| *Leave Disconnected* | Not used |

---

### Step 4: Wire the Profile Push Button
This button allows the wearer to cycle through profiles (Deaf $\to$ Deafblind $\to$ Blind $\to$ Language):
* Place the tactile push button across the center divider of the breadboard.
* Connect **Leg 1** to **ESP32 `GPIO 4`**.
* Connect **Leg 2** directly to **Breadboard Blue Rail (GND)**.
* *(No external resistor needed: ESP32 uses internal software `INPUT_PULLUP`).*

---

### Step 5: Wire the Active Buzzer (Blind Profile)
* Connect the **Positive pin (`+` or longer leg)** to **ESP32 `GPIO 26`**.
* Connect the **Negative pin (`-` or shorter leg)** to **Breadboard Blue Rail (GND)**.

---

### Step 6: Wire the 4 Status LEDs (Deaf Profile)
Each LED has a 220 Ω current-limiting resistor (Color code: Red-Red-Brown-Gold):

```
ESP32 GPIO ──► [220 Ω Resistor] ──► LED Anode (Long Leg) ──► LED Cathode (Short Leg) ──► GND Rail
```

| LED Color | Meaning | Resistor | ESP32 GPIO Pin |
| :--- | :--- | :--- | :--- |
| **Blue LED** | Platform Alert | 220 Ω | **`GPIO 13`** |
| **Orange / Yellow LED** | Train Delay | 220 Ω | **`GPIO 14`** |
| **Green LED** | Departure / Boarding | 220 Ω | **`GPIO 27`** |
| **Red LED** | Urgent Alert / Cancel | 220 Ω | **`GPIO 33`** |

---

## 4. Master GPIO Pinout Table

Print or screenshot this table for instant reference:

| ESP32 Pin | Connected Component | Wire / Leg | Function |
| :---: | :--- | :--- | :--- |
| **`GPIO 18`** | Servo $M1$ | Orange wire | Attention wave leader / Platform marker tap |
| **`GPIO 19`** | Servo $M2$ | Orange wire | Platform count taps / Delay alert taps |
| **`GPIO 23`** | Servo $M3$ | Orange wire | Departure long tap |
| **`GPIO 25`** | Servo $M4$ | Orange wire | Urgent rapid taps / Cancel tap |
| **`GPIO 21`** | MPU6050 SDA | Jumper | $I^2C$ Data line |
| **`GPIO 22`** | MPU6050 SCL | Jumper | $I^2C$ Clock line |
| **`GPIO 4`**  | Profile Switch | Button Pin 1 | Cycle Deaf/Deafblind/Blind/Language |
| **`GPIO 26`** | Active Buzzer | Buzzer (+) | Acoustic rhythm for blind profile |
| **`GPIO 13`** | Blue LED | Anode via 220 Ω | Platform visual indicator |
| **`GPIO 14`** | Orange/Yellow LED | Anode via 220 Ω | Delay visual indicator |
| **`GPIO 27`** | Green LED | Anode via 220 Ω | Departure visual indicator |
| **`GPIO 33`** | Red LED | Anode via 220 Ω | Urgent visual indicator |
| **`3.3V`**    | MPU6050 VCC | Jumper | 3.3V sensor logic power |
| **`GND`**     | Common Ground Rail | Jumper | Universal circuit ground |
| **`VIN`**     | L298N +5V Rail | Jumper | Untethered board power |

---

## 5. Mechanical Armband Assembly (The Physical Wearable)

```
        Wrist Strap / Armband Mount
  +---------------------------------------+
  |  [M1]     [M2]     [M3]     [M4]      |   <-- 4 Servos in a sequential line
  |   │        │        │        │        |
  |  ( )      ( )      ( )      ( )       |   <-- Single horn on each servo
  |                                       |
  |             [ MPU6050 ]               |   <-- Firmly taped to rigid strap
  +---------------------------------------+
```

1. **Servo Arrangement:**
   * Align the 4 servos in a straight sequential row on a cardboard backing, velcro strap, or 3D-printed band: $M1 \to M2 \to M3 \to M4$.
   * Spacing: roughly 2 to 3 cm apart.
2. **Servo Horns (Tappers):**
   * Attach a single straight arm (horn) to each servo.
   * **Rest Position:** The horn points sideways / retracted away from the wrist.
   * **Active Tap Position:** During a tap, the servo swings 35° inward so the tip of the plastic arm taps/pokes against the skin, then immediately snaps back.
   * **Clearance Check:** Ensure the horn does not physically hit the table or jam against the band frame. A stalled SG90 servo pulls > 800 mA and gets hot!
3. **Rigid MPU6050 Mounting (CRUCIAL):**
   * The MPU6050 **must be firmly taped or strapped to the rigid structure of the band**.
   * If the sensor hangs loosely by its jumper wires, wrist double-taps will NOT transfer kinetic energy to the accelerometer, and double-tap detection will fail!

---

## 6. Pre-Flight Bench Test (Verify Before Strapping On)

Once assembled, do these 4 quick bench tests to verify everything is 100% functional:

### Test 1: Power-On Twitch Test
1. Connect the ESP32 to your laptop via USB.
2. Connect the 9V battery to the L298N.
3. On boot, Servo $M1$ will give **one quick confirmation tap** (100 ms). If $M1$ moves, your power and servo signal are working!

### Test 2: Serial CLI Command Test (No Phone Needed!)
1. Open the Arduino Serial Monitor at **115200 baud**.
2. Type `A 1 2` and press Enter:
   * You will see the Attention wave: $M1 \to M2 \to M3 \to M4$ swing sequentially.
   * Then $M1$ gives a long tap, followed by two short taps on $M2$ (Platform 2).
   * Then the reverse wave $M4 \to M3 \to M2 \to M1$ signals message completed.
   * Blue LED lights up during the alert.
3. Type `P` and press Enter:
   * Profile cycles to DEAFBLIND $\to$ BLIND $\to$ LANGUAGE.
   * Green LED blinks to confirm.

### Test 3: Double-Tap Acknowledgment Test
1. Type `A 4 0` in the Serial Monitor (Urgent alert).
2. Servo $M4$ will rapidly tap 3 times and Red LED will flash. It repeats every 5 seconds.
3. Double-tap firmly with your fingers directly on the MPU6050 board.
4. Serial Monitor will print: `[TAP] Double-Tap confirmed!` and the pattern will immediately stop!

### Test 4: Web Bluetooth Phone Test
1. Open Chrome on Android: navigate to your deployed GitHub Pages URL (or laptop Chrome using the Mock/Serial transport).
2. Tap **Connect** $\to$ Select **EchoBand**.
3. Tap **Platform Alert** on the phone screen.
4. Watch the band play the tactile wave, then double-tap the sensor. The phone will instantly show **`ACKNOWLEDGED`**!

---

## 7. Troubleshooting Cheatsheet

| Issue | Cause | Fix |
| :--- | :--- | :--- |
| **Servos jitter or hum loudly** | Floating ground | Ensure the L298N `GND`, ESP32 `GND`, and Servo `Brown` wires are firmly seated in the common ground rail. |
| **ESP32 reboots when servo moves** | 9V battery voltage sag | 9V alkaline batteries drop voltage under heavy load. Keep the laptop USB plugged into the ESP32 for logic power while the 9V battery powers the servos. |
| **MPU6050 not detected (`0x68`)** | Loose I2C wires or floating AD0 | Verify `SDA` is in `GPIO 21`, `SCL` is in `GPIO 22`, and `AD0` is tied directly to `GND`. |
| **LED does not light up** | Reversed polarity or wrong resistor | Swap the LED legs (long leg must go to resistor/GPIO). Verify resistor color code is Red-Red-Brown (220 Ω) and NOT Red-Red-Yellow (220 kΩ). |
| **Double-tap doesn't register** | Sensor hanging loose | Securely tape the MPU6050 to the armband frame so physical wrist taps create sharp G-force shockwaves. |
