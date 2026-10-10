// station.ino — EchoBand Station Firmware
// Handles: Operator Buttons, Serial CLI, Web Bluetooth (NimBLE), ESP-NOW Broadcast

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include "protocol.h"

// --- Pin Definitions ---
static const uint8_t PIN_BTN_PLATFORM  = 32;
static const uint8_t PIN_BTN_DELAY     = 33;
static const uint8_t PIN_BTN_DEPARTURE = 25;
static const uint8_t PIN_BTN_URGENT    = 26;
static const uint8_t PIN_BUZZER        = 27; // Optional station buzzer

// --- State Variables ---
static uint16_t currentMsgId = 0;
static uint8_t  currentType  = T_NONE;
static uint8_t  currentArg   = 0;
static uint32_t alertTriggerTime = 0;

// Broadcast repeat management (3 times, 150ms apart)
static uint8_t  broadcastRepeatsRemaining = 0;
static uint32_t nextBroadcastMs = 0;
static EchoPacket pendingAlertPacket;

// Broadcast MAC (all FFs)
static uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// --- Band Registry ---
struct BandRecord {
  uint8_t  mac[6];
  uint8_t  id;          // Last byte of MAC
  uint32_t lastHelloMs; // millis() of last HELLO
  bool     received;    // Received currentMsgId
  bool     acked;       // Acknowledged currentMsgId
  bool     active;
};

#define MAX_BANDS 16
static BandRecord bands[MAX_BANDS];

// --- BLE References ---
static NimBLEServer*         pServer        = nullptr;
static NimBLECharacteristic* pNotifyChar    = nullptr;
static bool                  bleClientConnected = false;

// Serial Command Buffer
static String serialBuffer = "";

// Forward declarations
void broadcastPacket(const EchoPacket* pkt);
void triggerAlert(uint8_t type, uint8_t arg);
void publishStatus();
void processSerialLine(String line);

// --- Button Debouncing ---
struct DebouncedButton {
  uint8_t pin;
  uint8_t alertType;
  uint8_t defaultArg;
  bool lastState;
  uint32_t lastDebounceTime;
};

static DebouncedButton buttons[] = {
  { PIN_BTN_PLATFORM,  T_PLATFORM,  2, HIGH, 0 }, // Platform 2 default
  { PIN_BTN_DELAY,     T_DELAY,     0, HIGH, 0 },
  { PIN_BTN_DEPARTURE, T_DEPARTURE, 0, HIGH, 0 },
  { PIN_BTN_URGENT,    T_URGENT,    0, HIGH, 0 }
};

// --- BLE Callbacks ---
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override {
    bleClientConnected = true;
    Serial.println("[BLE] Client connected");
    publishStatus();
  }
  void onDisconnect(NimBLEServer* pServer) override {
    bleClientConnected = false;
    Serial.println("[BLE] Client disconnected");
    NimBLEDevice::startAdvertising();
  }
};

class WriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    std::string rxVal = pChar->getValue();
    if (rxVal.length() >= 2) {
      uint8_t type = (uint8_t)rxVal[0];
      uint8_t arg  = (uint8_t)rxVal[1];
      Serial.printf("[BLE RX] Alert command received: type=%u, arg=%u\n", type, arg);
      triggerAlert(type, arg);
    }
  }
};

// --- ESP-NOW Receive Callback (Compatibility for Core 2.x and 3.x) ---
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
void onDataReceive(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  const uint8_t *mac_addr = recv_info->src_addr;
#else
void onDataReceive(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
#endif
  if (len != sizeof(EchoPacket)) return;

  const EchoPacket *pkt = (const EchoPacket*)incomingData;
  if (pkt->magic != ECHO_MAGIC) return;

  uint8_t bandId = (pkt->bandId != 0) ? pkt->bandId : mac_addr[5];

  // Find or allocate band record
  int bandIndex = -1;
  for (int i = 0; i < MAX_BANDS; i++) {
    if (bands[i].active && memcmp(bands[i].mac, mac_addr, 6) == 0) {
      bandIndex = i;
      break;
    }
  }
  if (bandIndex == -1) {
    for (int i = 0; i < MAX_BANDS; i++) {
      if (!bands[i].active) {
        bandIndex = i;
        bands[i].active = true;
        memcpy(bands[i].mac, mac_addr, 6);
        bands[i].id = bandId;
        bands[i].received = false;
        bands[i].acked = false;
        break;
      }
    }
  }

  if (bandIndex != -1) {
    if (pkt->kind == K_HELLO) {
      bands[bandIndex].lastHelloMs = millis();
    } else if (pkt->kind == K_RECEIVED) {
      if (pkt->msgId == currentMsgId) {
        bands[bandIndex].received = true;
        Serial.printf("[ESP-NOW] Band 0x%02X confirmed RECEIVED for msg=%u\n", bandId, pkt->msgId);
        publishStatus();
      }
    } else if (pkt->kind == K_ACKED) {
      if (pkt->msgId == currentMsgId) {
        bands[bandIndex].acked = true;
        Serial.printf("[ESP-NOW] Band 0x%02X confirmed ACKED (double-tap) for msg=%u\n", bandId, pkt->msgId);
        publishStatus();
      }
    }
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== EchoBand Station Starting ===");

  // Buttons setup
  for (auto &btn : buttons) {
    pinMode(btn.pin, INPUT_PULLUP);
  }

  // Buzzer setup
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // 1. Wi-Fi & ESP-NOW initialization
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ECHO_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] Failed to initialize ESP-NOW!");
  } else {
    Serial.println("[OK] ESP-NOW initialized on Channel 1");
  }

  // Register broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = ECHO_WIFI_CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[WARN] Broadcast peer add failed (or already exists)");
  }

  // Register RX callback
  esp_now_register_recv_cb(onDataReceive);

  // 2. NimBLE setup
  NimBLEDevice::init("EchoStation");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Max TX power for station
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(ECHO_SERVICE_UUID);

  // Write Characteristic: 2 bytes [type, arg]
  NimBLECharacteristic* pWriteChar = pService->createCharacteristic(
    ECHO_CHAR_WRITE_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pWriteChar->setCallbacks(new WriteCallback());

  // Notify Characteristic: ASCII status line
  pNotifyChar = pService->createCharacteristic(
    ECHO_CHAR_NOTIFY_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  pService->start();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(ECHO_SERVICE_UUID);
  pAdv->setName("EchoStation");
  pAdv->start();
  Serial.println("[OK] NimBLE Advertising as 'EchoStation'");

  Serial.println("Type 'HELP' in Serial Monitor for test commands (e.g. 'A 1 2')\n");
  publishStatus();
}

// --- Main Loop ---
void loop() {
  uint32_t now = millis();

  // 1. Check Station Buttons
  for (auto &btn : buttons) {
    bool reading = digitalRead(btn.pin);
    if (reading != btn.lastState) {
      btn.lastDebounceTime = now;
    }
    if ((now - btn.lastDebounceTime) > 50) { // 50ms debounce
      if (reading == LOW && btn.lastState == HIGH) {
        Serial.printf("[BUTTON] Pressed on GPIO %u -> Triggering Type %u\n", btn.pin, btn.alertType);
        triggerAlert(btn.alertType, btn.defaultArg);
      }
    }
    btn.lastState = reading;
  }

  // 2. Repeat Broadcast Queue (3 times, 150ms apart)
  if (broadcastRepeatsRemaining > 0 && now >= nextBroadcastMs) {
    broadcastPacket(&pendingAlertPacket);
    broadcastRepeatsRemaining--;
    nextBroadcastMs = now + ECHO_BROADCAST_INTERVAL_MS;
  }

  // 3. Serial CLI line reader
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

  // 4. Periodic unconfirmed check & status update (every 2.5 seconds)
  static uint32_t lastPeriodicStatus = 0;
  if (now - lastPeriodicStatus >= 2500) {
    lastPeriodicStatus = now;
    publishStatus();
  }

  delay(10);
}

// --- Helper Functions ---

void triggerAlert(uint8_t type, uint8_t arg) {
  if (type < 1 || type > 5) return;

  currentMsgId++;
  currentType = type;
  currentArg  = arg;
  alertTriggerTime = millis();

  // Reset rx and ack states for this new alert
  for (int i = 0; i < MAX_BANDS; i++) {
    if (bands[i].active) {
      bands[i].received = false;
      bands[i].acked    = false;
    }
  }

  // Short buzzer beep
  digitalWrite(PIN_BUZZER, HIGH);
  delay(40);
  digitalWrite(PIN_BUZZER, LOW);

  // Prepare packet
  pendingAlertPacket.magic  = ECHO_MAGIC;
  pendingAlertPacket.kind   = K_ALERT;
  pendingAlertPacket.type   = type;
  pendingAlertPacket.arg    = arg;
  pendingAlertPacket.msgId  = currentMsgId;
  pendingAlertPacket.bandId = 0;

  // Send first transmission immediately
  broadcastPacket(&pendingAlertPacket);
  broadcastRepeatsRemaining = ECHO_BROADCAST_REPEATS - 1; // 2 repeats remain
  nextBroadcastMs = millis() + ECHO_BROADCAST_INTERVAL_MS;

  publishStatus();
}

void broadcastPacket(const EchoPacket* pkt) {
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t*)pkt, sizeof(EchoPacket));
  if (result != ESP_OK) {
    Serial.printf("[ESP-NOW] Send failed with code %d\n", result);
  }
}

void publishStatus() {
  uint32_t now = millis();
  uint8_t activeBandsCount = 0;
  uint8_t rxCount = 0;
  uint8_t ackCount = 0;
  String unconfirmedList = "";

  for (int i = 0; i < MAX_BANDS; i++) {
    if (bands[i].active) {
      // Active if heard via HELLO within last 15 seconds
      if (now - bands[i].lastHelloMs <= ECHO_UNCONFIRMED_TIMEOUT) {
        activeBandsCount++;
        if (bands[i].received) rxCount++;
        if (bands[i].acked)    ackCount++;

        // If alert was sent > 15s ago, or received but not acked
        if (currentMsgId > 0 && !bands[i].acked) {
          if (unconfirmedList.length() > 0) unconfirmedList += ",";
          char idBuf[8];
          snprintf(idBuf, sizeof(idBuf), "%02X", bands[i].id);
          unconfirmedList += idBuf;
        }
      }
    }
  }

  if (unconfirmedList.length() == 0) {
    unconfirmedList = "NONE";
  }

  // Format: msg=12 type=1 arg=2 bands=2 rx=2 ack=1 unconfirmed=A4
  char statusLine[128];
  snprintf(statusLine, sizeof(statusLine),
    "msg=%u type=%u arg=%u bands=%u rx=%u ack=%u unconfirmed=%s",
    currentMsgId, currentType, currentArg,
    activeBandsCount, rxCount, ackCount,
    unconfirmedList.c_str()
  );

  Serial.println(statusLine);

  if (bleClientConnected && pNotifyChar) {
    pNotifyChar->setValue((uint8_t*)statusLine, strlen(statusLine));
    pNotifyChar->notify();
  }
}

void processSerialLine(String line) {
  line.trim();
  if (line.equalsIgnoreCase("HELP")) {
    Serial.println("--- EchoBand Serial Commands ---");
    Serial.println("  A <type> <arg> : Send alert (e.g. 'A 1 2' for Platform 2)");
    Serial.println("                   Types: 1=Platform, 2=Delay, 3=Departure, 4=Urgent, 5=Cancelled");
    Serial.println("  STATUS         : Print live system status line");
    Serial.println("  HELP           : Show this help list");
    return;
  }

  if (line.equalsIgnoreCase("STATUS")) {
    publishStatus();
    return;
  }

  if (line.startsWith("A ") || line.startsWith("a ")) {
    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (firstSpace != -1) {
      uint8_t type = line.substring(firstSpace + 1, (secondSpace != -1) ? secondSpace : line.length()).toInt();
      uint8_t arg = (secondSpace != -1) ? line.substring(secondSpace + 1).toInt() : 0;
      Serial.printf("[SERIAL CLI] Parsed alert: type=%u arg=%u\n", type, arg);
      triggerAlert(type, arg);
      return;
    }
  }

  Serial.printf("[SERIAL CLI] Unknown command '%s'. Type HELP for syntax.\n", line.c_str());
}
