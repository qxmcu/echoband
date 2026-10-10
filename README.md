# EchoBand — One Message, Every Sense

> **HACK'TVM 2026** (Thiruvananthapuram)  
> *A wearable assistive armband that delivers public transit announcements to wearers through their own sense (touch, light, sound, or multilingual text/pictogram) with zero infrastructure.*

---

## 1. Problem Statement & Core Design Constraint

Public announcements in railway stations arrive through one acoustic channel. Deaf, deafblind, blind, and non-native speaking passengers miss them. 

### Core Design Constraint: ONE Board, Zero Infrastructure
* In the real world, transport authorities will not install custom hardware in stations for us.
* **The ONLY custom hardware is the Wearable Armband (1 single ESP32).**
* **The wearer's smartphone does everything else:** listens to existing loudspeaker announcements, transcribes them on-device using Whisper, matches alert keywords, displays pictograms and text in 3 languages, speaks aloud, and controls the armband over **Bluetooth Low Energy (Web Bluetooth)**.
* **There is NO station hardware, NO ESP-NOW, NO server, and NO cloud.**

---

## 2. System Architecture

```
 Loudspeaker announcement (the existing station PA)
        │ sound
        ▼
 PHONE (Chrome on Android, one static web page, no backend)
   mic → loudness gate → Whisper base multilingual (on device) → keyword matcher → {type, arg}
   on-screen buttons (manual fallback / primary demo path) ─────────────────────────┘
   shows pictogram + text (EN / ML / HI), speaks message (blind profile)
        │ Bluetooth Low Energy (Web Bluetooth, GATT write: 3 bytes)
        ▼
 BAND ESP32 ── 4 x SG90 servos (tap haptics M1..M4), active buzzer, LEDs
        ▲   MPU6050 (double-tap = acknowledgment), 1 profile button
        │
        └── GATT notify back to phone: RX (alert received), ACK (wearer double-tapped), PROFILE
```

### Two Input Pathways:
1. **Manual Buttons on the Phone Page (Primary, 100% Reliable Demo Path):** Five big buttons, one per alert type, plus a platform selector.
2. **Speech Recognition Path (Secondary, The Impressive AI Part):** The phone listens to announcements, transcribes them in-browser with `Xenova/whisper-base` (multilingual), and matches the alert.

---

## 3. Tactile Language Specification

The band uses 4 SG90 micro-servos ($M1$, $M2$, $M3$, $M4$) as **haptic tappers** (lever arm deflecting 35° against the arm). Servos move **one at a time** with a 50 ms stagger to prevent power spikes.

| Signal | Actuators Used | Rhythm & Physical Delivery |
| :--- | :--- | :--- |
| **Attention** *(Before every alert)* | $M1 \to M2 \to M3 \to M4$ | Sequential wave across wrist, 50 ms stagger |
| **Platform N** ($N = 1 \dots 5$) | $M1, M2$ | $M1$ long tap (600 ms), followed by $N$ short taps on $M2$ (200 ms each) |
| **Train Delay** | $M2$ | $M2$ two short taps |
| **Departure / Boarding** | $M3$ | $M3$ one long tap (600 ms) |
| **Urgent Alert** | $M4$ | $M4$ three rapid taps (150 ms); repeats every 5s until double-tap (max 6 repeats) |
| **Cancellation** | $M2, M4$ | $M2$ short tap, then $M4$ short tap |
| **Message Ended** *(Played last)* | $M4 \to M3 \to M2 \to M1$ | Reverse wave mirroring Attention |

---

## 4. Repository Layout

```
echoband/
├── common/
│   └── protocol.h             # BLE UUIDs and type codes
├── band/
│   ├── band.ino               # Band ESP32 firmware (NimBLE, non-blocking haptics, MPU6050)
│   └── protocol.h             # Sketch header include
├── phone/                     # Zero-framework static web application
│   ├── index.html             # Accessible, responsive mobile interface
│   ├── app.js                 # App coordinator and life-cycle manager
│   ├── protocol.js            # Mirrored UUIDs and constants for Web Bluetooth
│   ├── transport.js           # BLE, Web Serial, and Mock transport drivers
│   ├── audio.js               # Loudness gate, RMS analyser, and Screen Wake Lock
│   ├── asr.worker.js          # Web Worker running Xenova/whisper-base
│   ├── match.js               # Multilingual keyword matcher (EN, ML, HI)
│   ├── ui.js                  # Accessible trilingual cards, TTS readout, confirmation loop
│   ├── sw.js                  # Service Worker for 100% offline caching
│   └── assets/                # ARASAAC-style SVG pictograms (Platform, Delay, etc.)
├── tests/
│   └── match.test.js          # Node.js automated unit test suite
├── docs/
│   └── WIRING.md              # Complete hardware pinout and power guide
├── HARDWARE_GUIDE.md          # Visual assembly checklist for hardware teammate
├── package.json               # Test script and ES module declaration
└── README.md                  # This documentation
```

---

## 5. Hardware Assembly & Flashing

Read [HARDWARE_GUIDE.md](file:///C:/Users/jkosh/Downloads/echoband/HARDWARE_GUIDE.md) for full visual wiring steps.

### Required Libraries in Arduino IDE:
* **ESP32 Board Package** by Espressif
* **ESP32Servo** (Kevin Harrington)
* **NimBLE-Arduino** (h2zero)
* Built-in: `Wire`, `Preferences`

### Flashing the Band:
1. Connect the ESP32 via USB.
2. Open `band/band.ino` in Arduino IDE.
3. Select Board: **ESP32 Dev Module**, Upload Speed: **921600**.
4. Click Upload. Open Serial Monitor at **115200 baud**.
5. Test using Serial commands: `A 1 2` (Platform 2), `A 4 0` (Urgent), `P` (Cycle Profile).

---

## 6. Testing & Demonstration

### 1. Automated Software Unit Tests:
```bash
npm.cmd test
```
*(All 28 unit tests pass covering EN, ML, HI and priority rules).*

### 2. Zero-Hardware Web Simulator:
```bash
npx serve phone
```
Open `http://localhost:3000` in Chrome, select **Mock (Simulator)**, click **Connect**, and trigger alerts.

### 3. Live Phone Connection (Web Bluetooth):
* Serve `phone/` over **HTTPS** (e.g., GitHub Pages).
* Open in **Chrome on Android**.
* Tap **Connect** $\to$ choose **`EchoBand`**.
* Tap any manual alert button or speak an announcement.
* Double-tap the wrist sensor to confirm!
