// band.ino — EchoBand Wearable Band Firmware
// Non-blocking tactile engine, MPU6050 double-tap detector, Profiles, ESP-NOW

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include "protocol.h"

// --- Hardware Pin Definitions ---
static const uint8_t PIN_SERVO_M1    = 18;
static const uint8_t PIN_SERVO_M2    = 19;
static const uint8_t PIN_SERVO_M3    = 23;
static const uint8_t PIN_SERVO_M4    = 25;

static const uint8_t PIN_I2C_SDA     = 21;
static const uint8_t PIN_I2C_SCL     = 22;

static const uint8_t PIN_BTN_PROFILE = 4;
static const uint8_t PIN_BUZZER      = 26;

static const uint8_t PIN_LED_BLUE    = 13; // Platform
static const uint8_t PIN_LED_ORANGE  = 14; // Delay
static const uint8_t PIN_LED_GREEN   = 27; // Departure
static const uint8_t PIN_LED_RED     = 33; // Urgent / Cancel

// --- Servo Configuration ---
static const int SERVO_REST_ANGLE = 10;
static const int SERVO_TAP_ANGLE  = 45; // 35 deg deflection tap

static Servo servoM1;
static Servo servoM2;
static Servo servoM3;
static Servo servoM4;
static Servo* servos[4] = { &servoM1, &servoM2, &servoM3, &servoM4 };

// --- Tactile Timing Constants (ms) ---
static const uint16_t T_TAP_SHORT    = 200;
static const uint16_t T_TAP_LONG     = 600;
static const uint16_t T_TAP_RAPID    = 150;
static const uint16_t T_GAP          = 200;
static const uint16_t T_WAVE_STAGGER = 50;

// --- MPU6050 Constants ---
static const uint8_t  MPU6050_ADDR   = 0x68;
static const float    TAP_G_THRESH   = 2.5f; // Acceleration threshold (g)
static const uint32_t TAP_MIN_GAP_MS = 80;   // Min gap between taps
static const uint32_t TAP_MAX_GAP_MS = 600;  // Max gap between taps
static const uint32_t TAP_LOCKOUT_MS = 400;  // Lockout after valid double-tap

// --- Profile & Persistent State ---
static Preferences prefs;
static uint8_t     currentProfile = PROFILE_DEAF;
static uint8_t     bandId = 0;
static uint8_t     stationMac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static bool        hasStationMac = false;
static uint8_t     broadcastMac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Alert Tracking
static uint16_t lastMsgId = 0;
static uint8_t  activeAlertKind = T_NONE;
static uint8_t  activeAlertArg  = 0;
static bool     isAlertPlaying  = false;
static bool     isRepeating     = false;
static uint8_t  repeatCount     = 0;
static uint32_t nextRepeatMs    = 0;

// Button debounce
static bool     lastProfileBtnState = HIGH;
static uint32_t lastProfileBtnDebounce = 0;

// Double tap detector state
static uint32_t firstTapMs = 0;
static uint32_t lastDoubleTapMs = 0;
static uint32_t lastMpuSampleMs = 0;

// Heartbeat
static uint32_t lastHelloMs = 0;

// --- Non-Blocking Tactile Step Queue ---
struct TactileStep {
  int8_t   servoIdx;    // 0=M1, 1=M2, 2=M3, 3=M4, -1=None
  bool     active;      // true=tap position, false=rest position
  uint16_t durationMs;
  int8_t   ledPin;      // -1=None or LED pin
  bool     ledState;
  bool     buzzerState;
};

#define QUEUE_CAPACITY 64
static TactileStep queue[QUEUE_CAPACITY];
static uint8_t queueHead = 0;
static uint8_t queueTail = 0;
static uint8_t queueCount = 0;

static uint32_t currentStepStartMs = 0;
static uint16_t currentStepDuration = 0;
static int8_t   currentlyActiveServo = -1;

void clearQueue() {
  queueHead = 0;
  queueTail = 0;
  queueCount = 0;
  currentStepDuration = 0;
  // Return all servos to rest
  for (int i = 0; i < 4; i++) {
    servos[i]->write(SERVO_REST_ANGLE);
  }
  currentlyActiveServo = -1;
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_BLUE, LOW);
  digitalWrite(PIN_LED_ORANGE, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
}

bool pushStep(int8_t sIdx, bool act, uint16_t dur, int8_t led = -1, bool ledSt = false, bool buzz = false) {
  if (queueCount >= QUEUE_CAPACITY) return false;
  queue[queueTail].servoIdx    = sIdx;
  queue[queueTail].active      = act;
  queue[queueTail].durationMs  = dur;
  queue[queueTail].ledPin      = led;
  queue[queueTail].ledState    = ledSt;
  queue[queueTail].buzzerState = buzz;
  queueTail = (queueTail + 1) % QUEUE_CAPACITY;
  queueCount++;
  return true;
}

// Helper to push a tap + rest gap
void appendTap(int8_t sIdx, uint16_t tapMs, uint16_t gapMs = T_GAP, int8_t led = -1) {
  bool buzz = (currentProfile == PROFILE_BLIND);
  pushStep(sIdx, true,  tapMs, led, true, buzz);
  pushStep(sIdx, false, gapMs, led, false, false);
}

// Build sensory sequences
void buildPattern(uint8_t type, uint8_t arg) {
  clearQueue();

  int8_t ledPin = -1;
  if (currentProfile == PROFILE_DEAF) {
    if (type == T_PLATFORM)  ledPin = PIN_LED_BLUE;
    if (type == T_DELAY)     ledPin = PIN_LED_ORANGE;
    if (type == T_DEPARTURE) ledPin = PIN_LED_GREEN;
    if (type == T_URGENT || type == T_CANCELLED) ledPin = PIN_LED_RED;
  }

  // 1. Attention Wave: M1 -> M2 -> M3 -> M4
  pushStep(0, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(0, false, T_WAVE_STAGGER);
  pushStep(1, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(1, false, T_WAVE_STAGGER);
  pushStep(2, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(2, false, T_WAVE_STAGGER);
  pushStep(3, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(3, false, T_GAP);

  if (currentProfile == PROFILE_LANGUAGE) {
    // Language profile: minimal band output (one attention wave only)
    isAlertPlaying = true;
    return;
  }

  // 2. Alert Payload
  switch (type) {
    case T_PLATFORM: {
      uint8_t count = (arg >= 1 && arg <= 5) ? arg : 1;
      appendTap(0, T_TAP_LONG, T_GAP, ledPin); // M1 long tap
      for (uint8_t i = 0; i < count; i++) {
        appendTap(1, T_TAP_SHORT, T_GAP, ledPin); // N short taps on M2
      }
      break;
    }
    case T_DELAY: {
      appendTap(1, T_TAP_SHORT, T_GAP, ledPin); // M2 two short taps
      appendTap(1, T_TAP_SHORT, T_GAP, ledPin);
      break;
    }
    case T_DEPARTURE: {
      appendTap(2, T_TAP_LONG, T_GAP, ledPin);  // M3 long tap
      break;
    }
    case T_URGENT: {
      appendTap(3, T_TAP_RAPID, 80, ledPin);   // M4 three rapid taps
      appendTap(3, T_TAP_RAPID, 80, ledPin);
      appendTap(3, T_TAP_RAPID, T_GAP, ledPin);
      break;
    }
    case T_CANCELLED: {
      appendTap(1, T_TAP_SHORT, T_GAP, ledPin); // M2 then M4
      appendTap(3, T_TAP_SHORT, T_GAP, ledPin);
      break;
    }
  }

  // 3. Message Ended Wave (reverse of Attention): M4 -> M3 -> M2 -> M1
  pushStep(3, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(3, false, T_WAVE_STAGGER);
  pushStep(2, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(2, false, T_WAVE_STAGGER);
  pushStep(1, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(1, false, T_WAVE_STAGGER);
  pushStep(0, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(0, false, T_GAP);

  isAlertPlaying = true;
}

// Forward declarations
void sendPacket(uint8_t kind, uint16_t mId);
void cycleProfile();
bool initMPU6050();
void checkMPU6050();

// --- ESP-NOW Receive Callback ---
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
void onDataReceive(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  const uint8_t *mac_addr = recv_info->src_addr;
#else
void onDataReceive(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
#endif
  if (len != sizeof(EchoPacket)) return;

  const EchoPacket *pkt = (const EchoPacket*)incomingData;
  if (pkt->magic != ECHO_MAGIC) return;

  if (pkt->kind == K_ALERT) {
    // Save station MAC
    memcpy(stationMac, mac_addr, 6);
    hasStationMac = true;

    // Deduplicate
    if (pkt->msgId == lastMsgId) {
      return;
    }

    lastMsgId = pkt->msgId;
    activeAlertKind = pkt->type;
    activeAlertArg  = pkt->arg;

    Serial.printf("[ALERT RX] msgId=%u type=%u arg=%u\n", pkt->msgId, pkt->type, pkt->arg);

    // Send RECEIVED packet immediately
    sendPacket(K_RECEIVED, lastMsgId);

    // Build and play pattern
    buildPattern(activeAlertKind, activeAlertArg);

    // Check if repeat is required
    if (activeAlertKind == T_URGENT || currentProfile == PROFILE_DEAFBLIND) {
      isRepeating = true;
      repeatCount = 1;
      nextRepeatMs = millis() + ECHO_URGENT_REPEAT_MS;
    } else {
      isRepeating = false;
    }
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== EchoBand Wearable Starting ===");

  // Load saved profile from NVS
  prefs.begin("echoband", false);
  currentProfile = prefs.getUChar("profile", PROFILE_DEAF);
  if (currentProfile >= PROFILE_COUNT) currentProfile = PROFILE_DEAF;
  prefs.end();
  Serial.printf("[INIT] Current Profile: %u\n", currentProfile);

  // Pin modes
  pinMode(PIN_BTN_PROFILE, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_ORANGE, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);

  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_BLUE, LOW);
  digitalWrite(PIN_LED_ORANGE, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, LOW);

  // Attach servos (ESP32PWM timers)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoM1.setPeriodHertz(50);
  servoM2.setPeriodHertz(50);
  servoM3.setPeriodHertz(50);
  servoM4.setPeriodHertz(50);

  servoM1.attach(PIN_SERVO_M1, 500, 2400);
  servoM2.attach(PIN_SERVO_M2, 500, 2400);
  servoM3.attach(PIN_SERVO_M3, 500, 2400);
  servoM4.attach(PIN_SERVO_M4, 500, 2400);

  for (int i = 0; i < 4; i++) {
    servos[i]->write(SERVO_REST_ANGLE);
  }

  // Init MPU6050
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  if (initMPU6050()) {
    Serial.println("[OK] MPU6050 Accelerometer initialized (+/- 8g)");
  } else {
    Serial.println("[WARN] MPU6050 not detected on 0x68");
  }

  // Wi-Fi & ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ECHO_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  bandId = mac[5];
  Serial.printf("[INIT] Band ID: 0x%02X (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
    bandId, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] Failed to init ESP-NOW!");
  } else {
    Serial.println("[OK] ESP-NOW initialized on Channel 1");
  }

  // Add broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = ECHO_WIFI_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataReceive);

  // Send initial boot HELLO
  sendPacket(K_HELLO, 0);

  // Power-on confirmation tap
  appendTap(0, 100, 50);
}

// --- Main Loop ---
void loop() {
  uint32_t now = millis();

  // 1. Process Tactile Queue (Non-blocking)
  if (queueCount > 0) {
    if (currentStepDuration == 0) {
      // Start step
      TactileStep &step = queue[queueHead];
      currentStepStartMs = now;
      currentStepDuration = step.durationMs;

      // Servos: move only one at a time
      if (step.servoIdx >= 0 && step.servoIdx < 4) {
        if (step.active) {
          servos[step.servoIdx]->write(SERVO_TAP_ANGLE);
          currentlyActiveServo = step.servoIdx;
        } else {
          servos[step.servoIdx]->write(SERVO_REST_ANGLE);
          currentlyActiveServo = -1;
        }
      } else if (currentlyActiveServo >= 0) {
        servos[currentlyActiveServo]->write(SERVO_REST_ANGLE);
        currentlyActiveServo = -1;
      }

      // Buzzer
      digitalWrite(PIN_BUZZER, step.buzzerState ? HIGH : LOW);

      // LED
      if (step.ledPin > 0) {
        digitalWrite(step.ledPin, step.ledState ? HIGH : LOW);
      }
    } else if (now - currentStepStartMs >= currentStepDuration) {
      // Finished current step, advance queue
      TactileStep &step = queue[queueHead];
      if (step.ledPin > 0) digitalWrite(step.ledPin, LOW);
      digitalWrite(PIN_BUZZER, LOW);

      queueHead = (queueHead + 1) % QUEUE_CAPACITY;
      queueCount--;
      currentStepDuration = 0;

      if (queueCount == 0) {
        isAlertPlaying = false;
        if (currentlyActiveServo >= 0) {
          servos[currentlyActiveServo]->write(SERVO_REST_ANGLE);
          currentlyActiveServo = -1;
        }
      }
    }
  }

  // 2. Urgent / Deafblind Pattern Repeat
  if (isRepeating && !isAlertPlaying) {
    if (now >= nextRepeatMs) {
      if (repeatCount < ECHO_URGENT_MAX_REPEATS) {
        repeatCount++;
        Serial.printf("[REPEAT] Playing alert repeat %u / %u\n", repeatCount, ECHO_URGENT_MAX_REPEATS);
        buildPattern(activeAlertKind, activeAlertArg);
        nextRepeatMs = now + ECHO_URGENT_REPEAT_MS;
      } else {
        isRepeating = false; // Cap reached
      }
    }
  }

  // 3. MPU6050 Accelerometer Double-Tap Check (~200 Hz = 5 ms)
  if (now - lastMpuSampleMs >= 5) {
    lastMpuSampleMs = now;
    checkMPU6050();
  }

  // 4. Profile Button
  bool reading = digitalRead(PIN_BTN_PROFILE);
  if (reading != lastProfileBtnState) {
    lastProfileBtnDebounce = now;
  }
  if ((now - lastProfileBtnDebounce) > 50) {
    if (reading == LOW && lastProfileBtnState == HIGH) {
      cycleProfile();
    }
  }
  lastProfileBtnState = reading;

  // 5. Heartbeat HELLO (Every 5 seconds)
  if (now - lastHelloMs >= ECHO_HELLO_INTERVAL_MS) {
    lastHelloMs = now;
    sendPacket(K_HELLO, 0);
  }
}

// --- MPU6050 Raw I2C Implementation ---
bool initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // Wake up sensor (PWR_MGMT_1 = 0)
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  // Set range to +/- 8g (ACCEL_CONFIG = 0x10)
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  return true;
}

void checkMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return;

  if (Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)6) != 6) return;

  int16_t rawX = (Wire.read() << 8) | Wire.read();
  int16_t rawY = (Wire.read() << 8) | Wire.read();
  int16_t rawZ = (Wire.read() << 8) | Wire.read();

  // +/- 8g scale: 4096 LSB/g
  float ax = rawX / 4096.0f;
  float ay = rawY / 4096.0f;
  float az = rawZ / 4096.0f;
  float mag = sqrt(ax * ax + ay * ay + az * az);

  uint32_t now = millis();

  // Lockout check
  if (now - lastDoubleTapMs < TAP_LOCKOUT_MS) return;

  // Threshold spike check
  if (mag >= TAP_G_THRESH) {
    if (firstTapMs == 0) {
      // First spike
      firstTapMs = now;
    } else {
      // Second spike
      uint32_t dt = now - firstTapMs;
      if (dt >= TAP_MIN_GAP_MS && dt <= TAP_MAX_GAP_MS) {
        // Confirmed Double-Tap!
        lastDoubleTapMs = now;
        firstTapMs = 0;
        Serial.printf("[TAP] Double-Tap detected! (dt=%u ms, mag=%.2fg)\n", dt, mag);

        // Acknowledge alert
        if (lastMsgId > 0) {
          sendPacket(K_ACKED, lastMsgId);
          isRepeating = false;
          clearQueue();

          // Confirm tap on M1 + blink
          appendTap(0, 150, 100, PIN_LED_BLUE);
        }
      } else if (dt > TAP_MAX_GAP_MS) {
        // Too slow, restart first tap
        firstTapMs = now;
      }
    }
  } else if (firstTapMs > 0 && (now - firstTapMs > TAP_MAX_GAP_MS)) {
    // Window expired without second spike
    firstTapMs = 0;
  }
}

// --- Profile Switching ---
void cycleProfile() {
  currentProfile = (currentProfile + 1) % PROFILE_COUNT;

  prefs.begin("echoband", false);
  prefs.putUChar("profile", currentProfile);
  prefs.end();

  Serial.printf("[PROFILE] Switched to %u (0:Deaf, 1:Deafblind, 2:Blind, 3:Language)\n", currentProfile);

  clearQueue();
  // Feedback tap + blink count
  for (uint8_t i = 0; i <= currentProfile; i++) {
    appendTap(0, 80, 80, PIN_LED_GREEN);
  }
}

// --- Packet Transmission ---
void sendPacket(uint8_t kind, uint16_t mId) {
  EchoPacket pkt;
  pkt.magic  = ECHO_MAGIC;
  pkt.kind   = kind;
  pkt.type   = 0;
  pkt.arg    = 0;
  pkt.msgId  = mId;
  pkt.bandId = bandId;

  const uint8_t *target = hasStationMac ? stationMac : broadcastMac;

  // Ensure station peer exists in ESP-NOW
  if (hasStationMac && !esp_now_is_peer_exist(stationMac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, stationMac, 6);
    peerInfo.channel = ECHO_WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  esp_now_send(target, (const uint8_t*)&pkt, sizeof(EchoPacket));
}
