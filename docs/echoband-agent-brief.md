# EchoBand: briefing for an autonomous coding agent

You are helping a student team finish a hackathon project today (HACK'TVM 2026, Thiruvananthapuram, hackathon ends 3:00 PM IST). Read this whole brief, then produce working code, wiring notes and tests. Do not invent hardware that is not listed here. If something is ambiguous, pick the simplest option, state the assumption in a comment or README, and move on.

## 1. What we are building

**EchoBand** is a wearable armband plus a small "station" unit. The station sends a short coded alert, each armband delivers it to its wearer in the wearer's own sense (touch, light, sound, or text/pictogram in their language), and the wearer confirms with a double-tap. The station shows how many armbands received and confirmed the alert, so staff know who may still need help.

Problem statement being answered: **"One Message, Every Sense"**. Public announcements arrive through one channel, so people who are deaf, deafblind, blind or low vision, or who do not speak the announced language miss them. The statement also asks: how would anyone know a message was received, and who is still left out?

Setting chosen: a railway station. Alert types: platform change (with platform number), delay, departure/boarding, urgent, cancelled.

EchoBand adds a channel. It does not replace announcements, display boards, sign language or staff. It is not a certified safety system, and the code and docs must not claim otherwise.

## 2. System architecture

```
 Operator buttons (4) ──GPIO──┐
                              ▼
 Phone web page ──BLE (Web Bluetooth)──► STATION ESP32 ──ESP-NOW broadcast──► BAND ESP32 (one per wearer)
  mic → Whisper → keywords        ▲   │                                          │ servos M1..M4, buzzer, LEDs
  pictogram, text, speech         │   └── USB serial ──► laptop (counts display)  │ MPU6050 double-tap
                                  └──── status lines (BLE notify) ◄───────────────┘ RECEIVED / ACKED packets back
```

Two ways an alert enters the station:
1. **Operator path (primary, reliable, no speech recognition):** a hardware button on the station, or a button on the phone page.
2. **Speech path (secondary):** the phone listens, Whisper base (multilingual) transcribes, a keyword matcher picks the alert type and platform number, and the page sends it to the station.

The station broadcasts the alert over ESP-NOW. Each band plays the pattern for its stored profile, sends RECEIVED, repeats until the wearer double-taps, then sends ACKED. The station publishes status lines over BLE notify and USB serial.

## 3. Hardware available (verify before coding against it)

Treat this list as the only hardware.

- ESP32 dev boards: 2 (one band, one station). A third only if a second band is demoed.
- MPU6050 module: 1 per band (I2C, address 0x68).
- SG90 micro servos: 4 on the band (called M1, M2, M3, M4). Used as tap/poke haptic actuators (a lever arm on each), not as motors.
- Active buzzer module (band, blind profile) and a PCB buzzer (optional, station).
- LEDs (several colours) with 220 ohm resistors (verify these are 220 ohm and not 220 kilo-ohm); also 1 kilo-ohm and 10 kilo-ohm resistors, 10 each.
- Push buttons: about 5 (4 station presets, 1 band profile button).
- L298N motor driver board: used only as a **5V regulator** (see power).
- 9V battery, breadboards, jumper wires. Optional: RFID reader and tags. A spare Arduino UNO R4 WiFi exists but is not part of the design.
- Phone (Android, Chrome) and laptop (Chrome).

Not available, so do not design around them: microphone module, vibration (ERM) motor, power bank, capacitors, diodes, 330 ohm resistors, GSM, a real ultrasonic trigger. The microphone is the phone's.

### Power and wiring rules
- Band: 9V battery positive to the L298N 12V input, negative to its GND, 5V-enable jumper left on. The L298N 5V output feeds the servo rail (and the band ESP32 5V/VIN for untethered use). **All grounds joined** (battery, L298N, ESP32, servos, MPU6050).
- That regulator supplies roughly 0.5 A and gets warm. Therefore the firmware must **move only one servo at a time**, with a stagger of about 50 ms in any sequence.
- Station: powered from laptop USB.
- ESP32 logic is 3.3V. Servo signal wires go to GPIO directly; servo power is 5V from the L298N rail, never the 3.3V pin.
- Suggested pins (verify against the actual board): servos on GPIO 18, 19, 23, 25; MPU6050 SDA 21, SCL 22; band profile button GPIO 4 (INPUT_PULLUP); active buzzer GPIO 26; band LEDs on 13, 14, 27, 33 (each through a 220 ohm resistor). Avoid strapping pins (0, 2, 12, 15), flash pins (6 to 11) and input-only pins (34 to 39) for outputs. Station buttons on GPIO 32, 33, 25, 26 (INPUT_PULLUP, debounced in software).

## 4. Wire protocol (ESP-NOW)

Put this in a shared header (`common/protocol.h`) used by both firmwares.

```c
#define ECHO_MAGIC 0xEC
enum Kind : uint8_t { K_ALERT = 1, K_RECEIVED = 2, K_ACKED = 3, K_HELLO = 4 };
enum Type : uint8_t { T_PLATFORM = 1, T_DELAY = 2, T_DEPARTURE = 3, T_URGENT = 4, T_CANCELLED = 5 };
typedef struct __attribute__((packed)) {
  uint8_t  magic;   // ECHO_MAGIC
  uint8_t  kind;    // Kind
  uint8_t  type;    // Type (ALERT only, else 0)
  uint8_t  arg;     // platform number 1..5 for T_PLATFORM, else 0
  uint16_t msgId;   // station increments per new alert
  uint8_t  bandId;  // 0 in ALERT; band's id (last byte of its MAC) otherwise
} EchoPacket;
```

- Both boards: `WiFi.mode(WIFI_STA)`, no connection to any access point, **fixed channel 1**.
- Station broadcasts ALERT to FF:FF:FF:FF:FF:FF, repeated 3 times, 150 ms apart. Bands dedupe by `msgId`.
- Bands send HELLO at boot and every 5 s so the station knows which bands exist; they send RECEIVED when an alert arrives and ACKED on double-tap, addressed to the station MAC.
- A band that has RECEIVED but not ACKED after 15 s is reported as unconfirmed.
- The station also accepts alerts from USB serial lines such as `A 1 2` (type 1, arg 2) for testing without hardware.

## 5. Tactile language (band firmware)

Timing defaults (make them constants): short tap 200 ms, long tap 600 ms, gap 200 ms, wave stagger 50 ms. A "tap" is a servo swinging from rest by about 30 to 40 degrees and back. Position is secondary; **rhythm is the main code**.

| Signal | Pattern |
| --- | --- |
| Attention (played first before every alert) | Wave M1, M2, M3, M4 once |
| Platform N | M1 long tap, then N short taps on M2 (N = 1 to 5) |
| Delay | M2 two short taps |
| Departure / boarding | M3 long tap |
| Urgent | M4 three rapid taps; repeat every 5 s until double-tap (cap 6 repeats) |
| Cancelled | M2 then M4 |
| Message ended (played last) | Wave M4, M3, M2, M1 once |

Implement as a non-blocking state machine (no `delay()` in the main loop) so the MPU6050 and ESP-NOW callback stay responsive. Never move two servos at once.

### Profiles (stored in `Preferences`, cycled with the band button; confirm each change with a short distinct servo tap and LED blink)
- **DEAF:** servo pattern plus an LED colour (platform blue, delay orange, departure green, urgent red flashing, cancelled red double flash).
- **DEAFBLIND:** servo pattern only, repeated every 5 s until acknowledged.
- **BLIND:** buzzer tone pattern copying the same rhythm (short and long beeps); the phone page also speaks the message aloud.
- **LANGUAGE:** minimal band output (one attention wave); the phone page shows the pictogram and text in the chosen language.

### Acknowledgment
Read the MPU6050 accelerometer at 200 Hz or better (range plus or minus 8 g). A double-tap is two spikes where total acceleration exceeds about 2.5 g, 80 to 600 ms apart, with a 400 ms lockout afterward. Make thresholds constants and print live values on serial for tuning. On a valid double-tap: stop repeating, send ACKED, play one short confirm tap.

## 6. Phone web app (the part that runs on the phone)

One static, framework-free web app (plain HTML, CSS and JS modules). No backend. Audio is processed on the device and never stored or uploaded.

**How it executes on the phone**
1. The page is served over **HTTPS** (for example GitHub Pages or Netlify), because Web Bluetooth and `getUserMedia` need a secure context.
2. Open it in **Chrome on Android** (Web Bluetooth does not work on iOS). Allow the microphone.
3. Tap **Connect station**; Web Bluetooth requires a user gesture, and the station advertises as `EchoStation`.
4. Tap **Listen**. Keep the screen awake with the Screen Wake Lock API.
5. After the first load, a service worker caches the app shell and the Whisper model so it works offline.

**Modules**
- `transport.js`: one interface `send(type, arg)` and `onStatus(line)`, with three implementations: **BLE** (Web Bluetooth), **Serial** (Web Serial, for a laptop fallback) and **Mock** (for tests with no hardware).
- BLE: one service with a write characteristic (2 bytes: type, arg) and a notify characteristic carrying ASCII status lines. Use the NimBLE-Arduino library on the station.
- `audio.js`: microphone via `getUserMedia` with `echoCancellation`, `noiseSuppression`, `autoGainControl` on. A Web Audio `AnalyserNode` loudness gate starts capturing when level exceeds a threshold for about 300 ms and stops after about 700 ms of quiet or 10 s maximum. Also provide a push-to-listen button.
- `asr.worker.js`: runs `@huggingface/transformers` (transformers.js) with an automatic-speech-recognition pipeline on `Xenova/whisper-base` (the multilingual model, not the `.en` one), using WebGPU when available and WASM otherwise, in a Web Worker so the UI never freezes. Language is selectable (English, Malayalam, Hindi) or auto. The first load downloads the model (on the order of 100 MB or more), so show a progress bar and rely on the browser cache after.
- `match.js`: keyword matcher (below). Pure functions so it can be tested in Node.
- `ui.js`: big alert card with the pictogram (ARASAAC) and text in the chosen language, `speechSynthesis` readout for the blind profile, on-page buttons for all five alert types (these are the manual path), the live status line (alert sent, bands, received, acknowledged, unconfirmed band IDs), and a profile selector. Cache the five pictograms locally so they show offline.

### Keyword matcher rules
- Priority when several match: URGENT, CANCELLED, DELAY, PLATFORM, DEPARTURE.
- English keywords: urgent, emergency; cancel(l)ed; delay(ed), late; platform; depart, departure, boarding, leaving.
- Malayalam: അടിയന്തര, റദ്ദ്, വൈകി, പ്ലാറ്റ്ഫോം, പുറപ്പെടും. Hindi: आपात or तत्काल, रद्द, देरी, प्लेटफॉर्म, रवाना.
- Platform number: digits 1 to 5, or number words (one to five; Malayalam ഒന്ന് രണ്ട് മൂന്ന് നാല് അഞ്ച്; Hindi एक दो तीन चार पांच). Default to no number if none found.
- If nothing matches, do **nothing** except show the transcript and "no alert recognised". Never send URGENT on a weak or partial match.
- Display strings (have a native speaker check the Malayalam and Hindi before presenting):

| Type | English | Malayalam | Hindi |
| --- | --- | --- | --- |
| Platform | Platform {n} | പ്ലാറ്റ്ഫോം {n} | प्लेटफ़ॉर्म {n} |
| Delay | Train delayed | ട്രെയിൻ വൈകുന്നു | ट्रेन देरी से है |
| Departure | Train departing | ട്രെയിൻ പുറപ്പെടുന്നു | ट्रेन रवाना हो रही है |
| Urgent | Urgent announcement | അടിയന്തര അറിയിപ്പ് | तत्काल सूचना |
| Cancelled | Train cancelled | ട്രെയിൻ റദ്ദാക്കി | ट्रेन रद्द है |

### Known limits to state in the UI and README
Whisper base is small: expect weak accuracy in noisy rooms and especially in Malayalam. On a mid-range phone expect several seconds of delay per clip. That is why the operator path exists and why the speech path is a bonus. If the phone is too slow, run the same page on the laptop in Chrome using the Serial transport.

## 7. Station firmware responsibilities
- Read 4 debounced buttons (suggested: Platform with a configurable platform number defaulting to 2, Delay, Departure, Urgent; Cancelled is sent from the phone page). These mappings are adjustable.
- Receive alerts from BLE write, USB serial test lines and buttons, assign `msgId`, broadcast ESP-NOW.
- Track bands from HELLO packets; count RECEIVED and ACKED per `msgId`.
- Emit a status line on every change, over BLE notify and USB serial, in this format: `msg=12 type=1 arg=2 bands=2 rx=2 ack=1 unconfirmed=A4`.
- Optional: PCB buzzer beep on each send. ESP-NOW and BLE share one radio, so keep BLE traffic light; if they conflict, the serial transport is the fallback.

## 8. Repository layout and engineering rules
```
/common/protocol.h
/band/band.ino
/station/station.ino
/phone/index.html  app.js  transport.js  audio.js  asr.worker.js  match.js  ui.js  sw.js
/tests/match.test.js  (Node)
/docs/WIRING.md  README.md
```
- Arduino IDE compatible (ESP32 board package by Espressif). Libraries: ESP32Servo, NimBLE-Arduino, Wire, Preferences, esp_now, WiFi. Read the MPU6050 over raw I2C registers or the Adafruit library; either is fine.
- Keep code short and commented. Constants at the top of each file. No dynamic allocation in hot loops.
- Because you cannot touch hardware: build the Mock transport and the serial test lines so every path can be exercised without devices, and write Node unit tests for `match.js` using realistic phrases such as "The train to Ernakulam will depart from platform two", "Train 12625 is delayed by fifteen minutes", "Service cancelled", "അടിയന്തര അറിയിപ്പ്".
- WIRING.md lists every connection, the power arrangement and the pin table.

## 9. Definition of done
- Station button press makes a band play the correct servo pattern, then the band stops on a double-tap and the station shows rx and ack counts.
- All five alert types work from the buttons and from the phone page buttons.
- The phone speech path turns a played announcement into the right alert, or shows "no alert recognised".
- Profiles change band behaviour; the phone shows the pictogram and text in three languages.
- Everything runs without internet after the first load.

## 10. Non-goals and honesty rules
No cloud services, no accounts, no storing audio, no ultrasonic trigger (a roadmap idea only), no claim of medical or safety certification, no accuracy figures in the docs until real tests are run and recorded.
