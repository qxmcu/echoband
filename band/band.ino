// band.ino — EchoBand Wearable Armband Firmware (Single ESP32 Board Architecture)
// BLE Peripheral (NimBLE), Non-Blocking Tactile Engine, MPU6050 Double-Tap, Health Status LED, & Fallbacks

#include <Wire.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <NimBLEDevice.h>
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

// Output LEDs
static const uint8_t PIN_LED_BLUE    = 13; // Location / Platform
static const uint8_t PIN_LED_ORANGE  = 14; // Delay / Degraded Warning
static const uint8_t PIN_LED_GREEN   = 27; // Departure / Healthy System Status
static const uint8_t PIN_LED_RED     = 33; // Urgent / Critical Error Status
static const uint8_t PIN_LED_ONBOARD = 2;  // Built-in ESP32 Dev Board LED

// --- Servo Configuration ---
static const int SERVO_REST_ANGLE = 10;
static const int SERVO_TAP_ANGLE  = 45; // 35° outward swing

static Servo servoM1;
static Servo servoM2;
static Servo servoM3;
static Servo servoM4;
static Servo* servos[4] = { &servoM1, &servoM2, &servoM3, &servoM4 };

// --- MPU6050 Constants ---
static const uint8_t  MPU6050_ADDR   = 0x68;
static const float    TAP_G_THRESH   = 2.5f; // Threshold (g)
static const uint32_t TAP_MIN_GAP_MS = 80;   // Min gap between taps
static const uint32_t TAP_MAX_GAP_MS = 600;  // Max gap between taps
static const uint32_t TAP_LOCKOUT_MS = 400;  // Lockout after confirmed double-tap

// --- Health & Diagnostic State ---
static bool mpuHealthy     = false;
static bool bleHealthy     = false;
static bool bleConnected   = false;
static bool systemHealthy  = false;
static uint32_t lastHealthLedMs = 0;

// --- BLE References ---
static NimBLEServer*         pServer     = nullptr;
static NimBLECharacteristic* pStatusChar = nullptr;

// --- Profile & Persistent State ---
static Preferences prefs;
static uint8_t     currentProfile = PROFILE_DEAF;

// Alert State Tracking
static uint8_t  activeType      = ALERT_STOP;
static uint8_t  activeArg       = 0;
static uint8_t  activeMsgId     = 0;
static bool     isAlertPlaying  = false;
static bool     isRepeating     = false;
static uint8_t  repeatCount     = 0;
static uint32_t alertStartMs    = 0;
static uint32_t nextRepeatMs    = 0;

// Button debounce
static bool     lastProfileBtnState = HIGH;
static uint32_t lastProfileBtnDebounce = 0;

// Double tap detector state
static uint32_t firstTapMs = 0;
static uint32_t lastDoubleTapMs = 0;
static uint32_t lastMpuSampleMs = 0;

// Serial input buffer
static String serialBuffer = "";

// --- Non-Blocking Step Queue ---
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

void appendTap(int8_t sIdx, uint16_t tapMs, uint16_t gapMs = ECHOBAND_GAP_MS, int8_t led = -1) {
  bool buzz = (currentProfile == PROFILE_BLIND);
  pushStep(sIdx, true,  tapMs, led, true, buzz);
  pushStep(sIdx, false, gapMs, led, false, false);
}

// Forward declarations
void notifyStatus(const char* line);
void cycleProfile();
bool initMPU6050();
void checkMPU6050();
void processCommand(uint8_t type, uint8_t arg, uint8_t msgId);
void processSerialLine(String line);
const char* getProfileName(uint8_t p);
void updateHealthLeds(uint32_t now);
void triggerAcknowledge(uint8_t mId, const char* source);

// --- Build Sensory Patterns ---
void buildPattern(uint8_t type, uint8_t arg) {
  clearQueue();

  int8_t ledPin = -1;
  if (currentProfile == PROFILE_DEAF) {
    if (type == ALERT_PLATFORM)  ledPin = PIN_LED_BLUE;
    if (type == ALERT_DELAY)     ledPin = PIN_LED_ORANGE;
    if (type == ALERT_DEPARTURE) ledPin = PIN_LED_GREEN;
    if (type == ALERT_URGENT || type == ALERT_CANCELLED) ledPin = PIN_LED_RED;
  }

  // 1. Attention Wave: M1 -> M2 -> M3 -> M4 (50ms stagger)
  pushStep(0, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(0, false, ECHOBAND_WAVE_STAGGER_MS);
  pushStep(1, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(1, false, ECHOBAND_WAVE_STAGGER_MS);
  pushStep(2, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(2, false, ECHOBAND_WAVE_STAGGER_MS);
  pushStep(3, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(3, false, ECHOBAND_GAP_MS);

  if (currentProfile == PROFILE_LANGUAGE) {
    isAlertPlaying = true;
    return;
  }

  // 2. Alert Payload
  switch (type) {
    case ALERT_PLATFORM: {
      uint8_t count = (arg >= 1 && arg <= 5) ? arg : 1;
      appendTap(0, ECHOBAND_LONG_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      for (uint8_t i = 0; i < count; i++) {
        appendTap(1, ECHOBAND_SHORT_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      }
      break;
    }
    case ALERT_DELAY: {
      appendTap(1, ECHOBAND_SHORT_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      appendTap(1, ECHOBAND_SHORT_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      break;
    }
    case ALERT_DEPARTURE: {
      appendTap(2, ECHOBAND_LONG_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      break;
    }
    case ALERT_URGENT: {
      appendTap(3, ECHOBAND_RAPID_TAP_MS, 80, ledPin);
      appendTap(3, ECHOBAND_RAPID_TAP_MS, 80, ledPin);
      appendTap(3, ECHOBAND_RAPID_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      break;
    }
    case ALERT_CANCELLED: {
      appendTap(1, ECHOBAND_SHORT_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      appendTap(3, ECHOBAND_SHORT_TAP_MS, ECHOBAND_GAP_MS, ledPin);
      break;
    }
  }

  // 3. Message Ended Wave (Reverse of Attention)
  pushStep(3, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(3, false, ECHOBAND_WAVE_STAGGER_MS);
  pushStep(2, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(2, false, ECHOBAND_WAVE_STAGGER_MS);
  pushStep(1, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(1, false, ECHOBAND_WAVE_STAGGER_MS);
  pushStep(0, true,  120, ledPin, true, currentProfile == PROFILE_BLIND);
  pushStep(0, false, ECHOBAND_GAP_MS);

  isAlertPlaying = true;
}

// --- BLE Callbacks (Compatible with both NimBLE 1.x and modern NimBLE 2.x) ---
#if defined(NIMBLE_CPP_VERSION_MAJOR) && (NIMBLE_CPP_VERSION_MAJOR >= 2) || defined(NIMBLE_VERSION_MAJOR) && (NIMBLE_VERSION_MAJOR >= 2) || __has_include(<NimBLEConnInfo.h>)
// NimBLE-Arduino v2.x (Modern API)
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    bleConnected = true;
    Serial.println("[BLE] Phone connected to EchoBand!");
    char buf[32];
    snprintf(buf, sizeof(buf), "PROFILE %s", getProfileName(currentProfile));
    notifyStatus(buf);
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    bleConnected = false;
    Serial.println("[BLE] Phone disconnected -> Waiting for reconnect");
    NimBLEDevice::startAdvertising();
  }
};

class CommandCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    auto val = pChar->getValue();
    if (val.length() >= 3) {
      uint8_t type  = (uint8_t)val[0];
      uint8_t arg   = (uint8_t)val[1];
      uint8_t msgId = (uint8_t)val[2];
      Serial.printf("[BLE RX] Command: type=%u, arg=%u, msgId=%u\n", type, arg, msgId);
      processCommand(type, arg, msgId);
    }
  }
};
#else
// NimBLE-Arduino v1.x (Legacy API)
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override {
    bleConnected = true;
    Serial.println("[BLE] Phone connected to EchoBand!");
    char buf[32];
    snprintf(buf, sizeof(buf), "PROFILE %s", getProfileName(currentProfile));
    notifyStatus(buf);
  }
  void onDisconnect(NimBLEServer* pServer) override {
    bleConnected = false;
    Serial.println("[BLE] Phone disconnected -> Waiting for reconnect");
    NimBLEDevice::startAdvertising();
  }
};

class CommandCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    auto val = pChar->getValue();
    if (val.length() >= 3) {
      uint8_t type  = (uint8_t)val[0];
      uint8_t arg   = (uint8_t)val[1];
      uint8_t msgId = (uint8_t)val[2];
      Serial.printf("[BLE RX] Command: type=%u, arg=%u, msgId=%u\n", type, arg, msgId);
      processCommand(type, arg, msgId);
    }
  }
};
#endif

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n==================================");
  Serial.println("  EchoBand — Wearable Haptic Band ");
  Serial.println("==================================");

  // Pin initialization
  pinMode(PIN_BTN_PROFILE, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_ORANGE, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_ONBOARD, OUTPUT);

  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_BLUE, LOW);
  digitalWrite(PIN_LED_ORANGE, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_ONBOARD, LOW);

  // Load saved profile
  prefs.begin("echoband", false);
  currentProfile = prefs.getUChar("profile", PROFILE_DEAF);
  if (currentProfile >= PROFILE_COUNT) currentProfile = PROFILE_DEAF;
  prefs.end();
  Serial.printf("[INIT] Active Profile: %s (%u)\n", getProfileName(currentProfile), currentProfile);

  // Allocate PWM timers & attach servos
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

  // Initialize MPU6050
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  mpuHealthy = initMPU6050();
  if (mpuHealthy) {
    Serial.println("[OK] MPU6050 Accelerometer initialized (+/- 8g)");
  } else {
    Serial.println("[WARN] MPU6050 not detected at 0x68!");
    Serial.println("  ==> Fallback enabled: Press Profile Button (GPIO 4) to ACK alerts.");
  }

  // Initialize NimBLE peripheral as "EchoBand"
  NimBLEDevice::init("EchoBand");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(ECHOBAND_SERVICE_UUID);
  bleHealthy = (pService != nullptr);

  // Command Characteristic
  NimBLECharacteristic* pCmdChar = pService->createCharacteristic(
    ECHOBAND_CHAR_COMMAND_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pCmdChar->setCallbacks(new CommandCallback());

  // Status Characteristic
  pStatusChar = pService->createCharacteristic(
    ECHOBAND_CHAR_STATUS_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  pService->start();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(ECHOBAND_SERVICE_UUID);
  pAdv->setName("EchoBand");
  pAdv->start();
  Serial.println("[OK] BLE Advertising as 'EchoBand'");

  systemHealthy = mpuHealthy && bleHealthy;
  Serial.printf("[DIAGNOSTICS] Health: %s (MPU=%s, BLE=%s)\n",
    systemHealthy ? "OPTIMAL" : "DEGRADED (FALLBACKS ACTIVE)",
    mpuHealthy ? "OK" : "OFFLINE",
    bleHealthy ? "OK" : "ERROR");

  // Initial confirmation tap on M1
  appendTap(0, 100, 50);
}

// --- Main Loop ---
void loop() {
  uint32_t now = millis();

  // 1. Process Tactile Step Queue (Non-blocking)
  if (queueCount > 0) {
    if (currentStepDuration == 0) {
      TactileStep &step = queue[queueHead];
      currentStepStartMs = now;
      currentStepDuration = step.durationMs;

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

      digitalWrite(PIN_BUZZER, step.buzzerState ? HIGH : LOW);
      if (step.ledPin > 0) {
        digitalWrite(step.ledPin, step.ledState ? HIGH : LOW);
      }
    } else if (now - currentStepStartMs >= currentStepDuration) {
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

  // 2. Alert Repeat Rules
  if (isRepeating && !isAlertPlaying) {
    if (now - alertStartMs > ECHOBAND_SAFETY_CUTOFF_MS) {
      Serial.println("[REPEAT] Safety cutoff reached (60s). Stopping repeat.");
      isRepeating = false;
    } else if (now >= nextRepeatMs) {
      if (activeType == ALERT_URGENT || currentProfile == PROFILE_DEAFBLIND) {
        if (repeatCount < ECHOBAND_MAX_REPEATS) {
          repeatCount++;
          Serial.printf("[REPEAT] Playing repeat %u / %u\n", repeatCount, ECHOBAND_MAX_REPEATS);
          buildPattern(activeType, activeArg);
          nextRepeatMs = now + ECHOBAND_URGENT_REPEAT_MS;
        } else {
          isRepeating = false;
        }
      } else {
        if (repeatCount < 1) {
          repeatCount++;
          Serial.println("[REPLAY] Replaying alert once after 5s unconfirmed.");
          buildPattern(activeType, activeArg);
        }
        isRepeating = false;
      }
    }
  }

  // 3. MPU6050 Accelerometer Double-Tap Check
  if (mpuHealthy && (now - lastMpuSampleMs >= 5)) {
    lastMpuSampleMs = now;
    checkMPU6050();
  }

  // 4. Profile / ACK Button Check (With Hardware Fallback!)
  bool reading = digitalRead(PIN_BTN_PROFILE);
  if (reading != lastProfileBtnState) {
    lastProfileBtnDebounce = now;
  }
  if ((now - lastProfileBtnDebounce) > 50) {
    if (reading == LOW && lastProfileBtnState == HIGH) {
      if (isAlertPlaying || isRepeating) {
        // FALLBACK: When alert is active, pressing the button ACKs the alert!
        triggerAcknowledge(activeMsgId, "Button Fallback");
      } else {
        // Normal state: cycle profile
        cycleProfile();
      }
    }
  }
  lastProfileBtnState = reading;

  // 5. System Health Status LED Manager (When no alert is actively firing)
  if (!isAlertPlaying && queueCount == 0) {
    updateHealthLeds(now);
  }

  // 6. USB Serial CLI Parser
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processSerialLine(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }

  delay(5);
}

// --- System Health Status LED Logic ---
void updateHealthLeds(uint32_t now) {
  // Update LED states every 200ms
  if (now - lastHealthLedMs < 200) return;
  lastHealthLedMs = now;

  // Determine current health
  systemHealthy = mpuHealthy && bleHealthy;

  if (systemHealthy && bleConnected) {
    // HEALTHY & CONNECTED: Green LED and Onboard LED stay SOLID ON
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_ONBOARD, HIGH);
    digitalWrite(PIN_LED_ORANGE, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  } else if (!mpuHealthy && !bleHealthy) {
    // CRITICAL FAULT: Red LED stays solid ON
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_ORANGE, LOW);
    digitalWrite(PIN_LED_ONBOARD, LOW);
  } else {
    // DEGRADED / WAITING: Orange LED & Onboard LED BLINK slowly (1 Hz)
    static bool blinkState = false;
    blinkState = !blinkState;
    digitalWrite(PIN_LED_ORANGE, blinkState ? HIGH : LOW);
    digitalWrite(PIN_LED_ONBOARD, blinkState ? HIGH : LOW);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  }
}

// --- Trigger Acknowledgment (Sensor or Button Fallback) ---
void triggerAcknowledge(uint8_t mId, const char* source) {
  Serial.printf("[ACK] Alert confirmed via %s! (msgId=%u)\n", source, mId);
  isRepeating = false;
  clearQueue();

  char ackBuf[32];
  snprintf(ackBuf, sizeof(ackBuf), "ACK %u", mId);
  notifyStatus(ackBuf);

  // Confirmation tap on M1 + Blue LED blink
  appendTap(0, 150, 100, PIN_LED_BLUE);
}

// --- Command Processor ---
void processCommand(uint8_t type, uint8_t arg, uint8_t msgId) {
  if (type == ALERT_STOP) {
    Serial.println("[CMD] STOP received. Dismissing alert.");
    clearQueue();
    isAlertPlaying = false;
    isRepeating    = false;
    activeType     = ALERT_STOP;
    char ackBuf[32];
    snprintf(ackBuf, sizeof(ackBuf), "ACK %u", msgId);
    notifyStatus(ackBuf);
    return;
  }

  if (msgId != 0 && msgId == activeMsgId) {
    Serial.printf("[CMD] Duplicate msgId %u ignored.\n", msgId);
    return;
  }

  activeType     = type;
  activeArg      = arg;
  activeMsgId    = msgId;
  alertStartMs   = millis();
  repeatCount    = 0;

  // Notify Phone: RX <msgId>
  char rxBuf[32];
  snprintf(rxBuf, sizeof(rxBuf), "RX %u", msgId);
  notifyStatus(rxBuf);

  buildPattern(activeType, activeArg);

  isRepeating  = true;
  nextRepeatMs = millis() + ECHOBAND_URGENT_REPEAT_MS;
}

// --- MPU6050 Raw I2C Implementation ---
bool initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  if (Wire.endTransmission() != 0) return false;

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10); // +/- 8g
  if (Wire.endTransmission() != 0) return false;

  return true;
}

void checkMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    mpuHealthy = false;
    return;
  }

  if (Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)6) != 6) {
    mpuHealthy = false;
    return;
  }

  int16_t rawX = (Wire.read() << 8) | Wire.read();
  int16_t rawY = (Wire.read() << 8) | Wire.read();
  int16_t rawZ = (Wire.read() << 8) | Wire.read();

  float ax = rawX / 4096.0f;
  float ay = rawY / 4096.0f;
  float az = rawZ / 4096.0f;
  float mag = sqrt(ax * ax + ay * ay + az * az);

  uint32_t now = millis();
  if (now - lastDoubleTapMs < TAP_LOCKOUT_MS) return;

  if (mag >= TAP_G_THRESH) {
    if (firstTapMs == 0) {
      firstTapMs = now;
    } else {
      uint32_t dt = now - firstTapMs;
      if (dt >= TAP_MIN_GAP_MS && dt <= TAP_MAX_GAP_MS) {
        lastDoubleTapMs = now;
        firstTapMs = 0;
        triggerAcknowledge(activeMsgId, "MPU6050 Double-Tap");
      } else if (dt > TAP_MAX_GAP_MS) {
        firstTapMs = now;
      }
    }
  } else if (firstTapMs > 0 && (now - firstTapMs > TAP_MAX_GAP_MS)) {
    firstTapMs = 0;
  }
}

// --- Profile Switching ---
void cycleProfile() {
  currentProfile = (currentProfile + 1) % PROFILE_COUNT;

  prefs.begin("echoband", false);
  prefs.putUChar("profile", currentProfile);
  prefs.end();

  const char* pName = getProfileName(currentProfile);
  Serial.printf("[PROFILE] Switched to %s (%u)\n", pName, currentProfile);

  char profBuf[32];
  snprintf(profBuf, sizeof(profBuf), "PROFILE %s", pName);
  notifyStatus(profBuf);

  clearQueue();
  for (uint8_t i = 0; i <= currentProfile; i++) {
    appendTap(0, 80, 80, PIN_LED_GREEN);
  }
}

const char* getProfileName(uint8_t p) {
  switch (p) {
    case PROFILE_DEAF:      return PROFILE_NAME_DEAF;
    case PROFILE_DEAFBLIND: return PROFILE_NAME_DEAFBLIND;
    case PROFILE_BLIND:     return PROFILE_NAME_BLIND;
    case PROFILE_LANGUAGE:  return PROFILE_NAME_LANGUAGE;
    default:                return "UNKNOWN";
  }
}

void notifyStatus(const char* line) {
  if (bleConnected && pStatusChar) {
    pStatusChar->setValue((uint8_t*)line, strlen(line));
    pStatusChar->notify();
  }
}

// --- Serial CLI Handler ---
void processSerialLine(String line) {
  line.trim();
  if (line.equalsIgnoreCase("P")) {
    cycleProfile();
    return;
  }
  if (line.equalsIgnoreCase("STATUS")) {
    Serial.printf("State: activeType=%u, msgId=%u, profile=%s, ble=%s, mpu=%s\n",
      activeType, activeMsgId, getProfileName(currentProfile),
      bleConnected ? "CONNECTED" : "WAITING",
      mpuHealthy ? "HEALTHY" : "OFFLINE");
    return;
  }
  if (line.startsWith("A ") || line.startsWith("a ")) {
    int s1 = line.indexOf(' ');
    int s2 = line.indexOf(' ', s1 + 1);
    int s3 = (s2 != -1) ? line.indexOf(' ', s2 + 1) : -1;

    uint8_t type  = line.substring(s1 + 1, (s2 != -1) ? s2 : line.length()).toInt();
    uint8_t arg   = (s2 != -1) ? line.substring(s2 + 1, (s3 != -1) ? s3 : line.length()).toInt() : 0;
    static uint8_t serialMsgSeq = 100;
    uint8_t msgId = (s3 != -1) ? line.substring(s3 + 1).toInt() : serialMsgSeq++;

    Serial.printf("[SERIAL CLI] Parsed alert: type=%u, arg=%u, msgId=%u\n", type, arg, msgId);
    processCommand(type, arg, msgId);
    return;
  }
  Serial.println("Commands: 'A <type> <arg> [msgId]' (e.g. 'A 1 2'), 'P' (Cycle Profile), 'STATUS'");
}
