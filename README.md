# EchoBand — One Message, Every Sense

> **HACK'TVM 2026** (Thiruvananthapuram)  
> *A multi-sensory assistive wearable and station broadcast system bridging railway announcements across tactile, visual, auditory, and multilingual barriers.*

---

## 1. Problem Statement & Overview

Public transit terminals broadcast critical schedule and emergency announcements through a single acoustic channel. As a result:
* **Deaf and hard-of-hearing** passengers miss loudspeaker announcements.
* **Blind and low-vision** passengers cannot read digital display boards or platform signages.
* **Deafblind** passengers miss both acoustic announcements and visual screens.
* **Non-native speakers or travelers** unfamiliar with the local language cannot understand urgent verbal updates.
* **Station staff have no feedback loop:** They cannot know who received an alert and who remains uninformed in an emergency.

**EchoBand** solves this by establishing a decentralized, multi-sensory alert bridge:
1. **Multi-Channel Reception:** A station unit broadcasts coded alerts over **ESP-NOW**.
2. **Wearer's Own Sense:** Each wearable armband delivers the alert through the wearer’s chosen sensory modality:
   * **Deaf:** 4 sequential servo taps + distinct LED color code.
   * **Deafblind:** Tactile rhythm waves across 4 micro servos, repeated every 5 seconds until confirmed.
   * **Blind:** Acoustic buzzer rhythm copying the tactile pattern, plus spoken phone readout via Text-to-Speech.
   * **Language / Cognitive:** Clear ARASAAC-style pictograms and trilingual display (English, Malayalam, Hindi).
3. **Closing the Loop (Double-Tap Confirmation):** The wearer acknowledges the alert with a quick **double-tap** on the armband (detected by an onboard **MPU6050 accelerometer** at 200 Hz). The band transmits an acknowledgment back to the station, providing live operator counts of received vs. unconfirmed wearers.

---

## 2. System Architecture

```
  Station Operator Buttons (4) ──GPIO──┐
                                       ▼
  Phone Web App (PWA) ──BLE (Web Bluetooth)──► STATION ESP32 ──ESP-NOW Broadcast──► BAND ESP32 (Wearable)
   ├─ Mic → Whisper ASR → Matcher      ▲       │ (Channel 1, broadcast MAC)         ├─ Servos M1..M4 (haptics)
   ├─ Pictogram & Trilingual Text      │       │                                    ├─ MPU6050 (double-tap ACK)
   └─ Screen Wake Lock & TTS           │       └── USB Serial ──► Laptop Monitor     ├─ Buzzer & 4 Color LEDs
                                       └──────── BLE Notify ◄───────────────────────┴─ Battery + L298N 5V Rail
                                                  (Live status & unconfirmed counts)
```

### Two Input Pathways:
1. **Operator Path (Primary & 100% Reliable):** 4 physical hardware buttons on the Station ESP32, or 5 touch buttons on the Phone Web Page. Works instantaneously with zero network or speech recognition lag.
2. **Speech Recognition Path (Secondary & Bonus):** The phone listens to terminal announcements, transcribes them in-browser using `Xenova/whisper-base` (multilingual), extracts alert type and platform number via a deterministic keyword matcher, and broadcasts the event.

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
│   └── protocol.h             # 7-byte packed wire protocol for ESP-NOW and BLE
├── station/
│   ├── station.ino            # Station ESP32 firmware (NimBLE, ESP-NOW, buttons, CLI)
│   └── protocol.h             # Local header include
├── band/
│   ├── band.ino               # Band ESP32 firmware (Non-blocking haptics, MPU6050, profiles)
│   └── protocol.h             # Local header include
├── phone/                     # Zero-framework static web application
│   ├── index.html             # Accessible, responsive mobile interface
│   ├── app.js                 # App coordinator and life-cycle manager
│   ├── transport.js           # BLE, Web Serial, and Mock transport drivers
│   ├── audio.js               # Loudness gate, RMS analyser, and Screen Wake Lock
│   ├── asr.worker.js          # Web Worker running Xenova/whisper-base
│   ├── match.js               # Multilingual keyword matcher (EN, ML, HI)
│   ├── ui.js                  # Accessible trilingual cards, TTS readout, status monitor
│   ├── sw.js                  # Service Worker for 100% offline caching
│   └── assets/                # ARASAAC-style SVG pictograms (Platform, Delay, etc.)
├── tests/
│   └── match.test.js          # Node.js automated unit test suite
├── docs/
│   └── WIRING.md              # Complete hardware pinout and power guide
├── HARDWARE_GUIDE.md          # Shareable assembly checklist for hardware teammate
├── package.json               # Test script and ES module declaration
└── README.md                  # This documentation
```

---

## 5. Hardware Setup & Flashing Guide

Detailed wiring diagrams, pin tables, and power schematics are documented in [docs/WIRING.md](file:///C:/Users/jkosh/Downloads/echoband/docs/WIRING.md) and [HARDWARE_GUIDE.md](file:///C:/Users/jkosh/Downloads/echoband/HARDWARE_GUIDE.md).

### Required Libraries in Arduino IDE:
* **ESP32 Board Package** by Espressif (`v2.x` or `v3.x`)
* **ESP32Servo** (Kevin Harrington)
* **NimBLE-Arduino** (h2zero)
* Built-in: `WiFi`, `esp_now`, `Wire`, `Preferences`

### Flashing Station:
1. Connect Station ESP32 via USB.
2. Open `station/station.ino` in Arduino IDE.
3. Select Board: **ESP32 Dev Module**, Upload Speed: **921600**.
4. Upload. Open Serial Monitor at **115200 baud**.

### Flashing Band:
1. Connect Band ESP32 via USB.
2. Open `band/band.ino` in Arduino IDE.
3. Select Board: **ESP32 Dev Module**.
4. Upload. Open Serial Monitor at **115200 baud**.

---

## 6. Testing & Demonstration

### Automated Software Unit Tests:
Run the keyword matcher test suite covering English, Malayalam, Hindi, and priority resolution:
```bash
npm test
# or
node tests/match.test.js
```
*(All 28 automated tests pass out of the box).*

### Hardware-Free Web Simulation:
1. Serve the `phone/` directory using any static file server:
   ```bash
   npx serve phone
   ```
2. Open `http://localhost:3000` in Chrome.
3. Leave transport on **Mock (Simulator)** and click **Connect**.
4. Click any alert button (e.g. *Platform Alert* or *Urgent Alert*).
5. Watch the alert card display the multilingual text and pictogram, the status line show `rx=1`, and after 3.5 seconds simulate the double-tap to show `ack=1` and `unconfirmed=NONE`.

### Station Serial CLI Commands:
You can control and test the station directly from the Arduino Serial Monitor without hardware:
* `A 1 2` — Broadcast Platform 2 alert
* `A 2 0` — Broadcast Train Delay alert
* `A 3 0` — Broadcast Departure alert
* `A 4 0` — Broadcast Urgent announcement
* `A 5 0` — Broadcast Service Cancelled alert
* `STATUS` — Print live system status string

---

## 7. Known Limits & Honesty Statement

* **Safety Certification:** EchoBand is an accessibility prototype for hackathon demonstration. It is **not** a certified life-safety system and does not replace official transit staff or emergency protocols.
* **Whisper ASR Latency & Accuracy:** `Xenova/whisper-base` runs locally on client hardware via WebGPU/WASM. On mid-range smartphones or in loud transit stations, expect transcription latency of several seconds and reduced accuracy in regional languages like Malayalam. This is why the **physical operator buttons** remain the primary reliable pathway.
* **Power & Thermal Limits:** The L298N regulator supplies ~0.5 A. Firmwares enforce sequential servo activation (never concurrent) to prevent brownouts.
* **Privacy:** All audio captured is processed directly on the client device inside the Web Worker and immediately discarded. No audio or personal data is ever recorded, stored, or transmitted to any server.
