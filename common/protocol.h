#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

#include <stdint.h>

#define ECHO_MAGIC 0xEC
#define ECHO_WIFI_CHANNEL 1

// BLE UUIDs for EchoStation peripheral
#define ECHO_SERVICE_UUID        "0000ec00-0000-1000-8000-00805f9b34fb"
#define ECHO_CHAR_WRITE_UUID    "0000ec01-0000-1000-8000-00805f9b34fb"
#define ECHO_CHAR_NOTIFY_UUID   "0000ec02-0000-1000-8000-00805f9b34fb"

// Packet Kinds
enum Kind : uint8_t {
  K_ALERT    = 1,
  K_RECEIVED = 2,
  K_ACKED    = 3,
  K_HELLO    = 4
};

// Alert Types
enum Type : uint8_t {
  T_NONE      = 0,
  T_PLATFORM  = 1,
  T_DELAY     = 2,
  T_DEPARTURE = 3,
  T_URGENT    = 4,
  T_CANCELLED = 5
};

// Wearer Profiles (Band internal state)
enum Profile : uint8_t {
  PROFILE_DEAF      = 0,
  PROFILE_DEAFBLIND = 1,
  PROFILE_BLIND     = 2,
  PROFILE_LANGUAGE  = 3,
  PROFILE_COUNT     = 4
};

// Wire packet over ESP-NOW (packed 7 bytes)
typedef struct __attribute__((packed)) {
  uint8_t  magic;   // Must be ECHO_MAGIC (0xEC)
  uint8_t  kind;    // Kind (K_ALERT, K_RECEIVED, K_ACKED, K_HELLO)
  uint8_t  type;    // Type (ALERT only, else 0)
  uint8_t  arg;     // platform number 1..5 for T_PLATFORM, else 0
  uint16_t msgId;   // station increments per new alert
  uint8_t  bandId;  // 0 in ALERT; band's id (last byte of its MAC) otherwise
} EchoPacket;

// Timing & System Constants
#define ECHO_BROADCAST_REPEATS       3     // Station repeats broadcast
#define ECHO_BROADCAST_INTERVAL_MS 150     // Ms between repeat broadcasts
#define ECHO_HELLO_INTERVAL_MS    5000     // Band hello heartbeat
#define ECHO_UNCONFIRMED_TIMEOUT 15000     // 15 seconds unconfirmed threshold
#define ECHO_URGENT_REPEAT_MS     5000     // Urgent alert tactile repeat gap
#define ECHO_URGENT_MAX_REPEATS      6     // Capped at 6 repeats

#endif // COMMON_PROTOCOL_H
