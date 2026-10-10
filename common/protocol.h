#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

#include <stdint.h>

// --- BLE 128-bit Custom UUIDs ---
#define ECHOBAND_SERVICE_UUID      "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define ECHOBAND_CHAR_COMMAND_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // Write (Phone -> Band, 3 bytes)
#define ECHOBAND_CHAR_STATUS_UUID  "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // Notify (Band -> Phone, ASCII)

// --- Alert Types (Byte 0) ---
enum AlertType : uint8_t {
  ALERT_STOP       = 0, // Stop / Cancel current alert pattern
  ALERT_PLATFORM   = 1, // Platform change (arg = 1..5)
  ALERT_DELAY      = 2, // Train delayed
  ALERT_DEPARTURE  = 3, // Boarding / Departure
  ALERT_URGENT     = 4, // Urgent announcement
  ALERT_CANCELLED  = 5  // Train cancelled
};

// --- Wearer Profiles ---
enum BandProfile : uint8_t {
  PROFILE_DEAF      = 0,
  PROFILE_DEAFBLIND = 1,
  PROFILE_BLIND     = 2,
  PROFILE_LANGUAGE  = 3,
  PROFILE_COUNT     = 4
};

// Profile String Names
#define PROFILE_NAME_DEAF      "DEAF"
#define PROFILE_NAME_DEAFBLIND "DEAFBLIND"
#define PROFILE_NAME_BLIND     "BLIND"
#define PROFILE_NAME_LANGUAGE  "LANGUAGE"

// Timing & System Constants
#define ECHOBAND_SHORT_TAP_MS     200
#define ECHOBAND_LONG_TAP_MS      600
#define ECHOBAND_RAPID_TAP_MS     150
#define ECHOBAND_GAP_MS           200
#define ECHOBAND_WAVE_STAGGER_MS   50

#define ECHOBAND_ACK_TIMEOUT_MS  15000 // 15 seconds to acknowledge
#define ECHOBAND_URGENT_REPEAT_MS 5000 // Repeat every 5 seconds
#define ECHOBAND_MAX_REPEATS         6 // Cap repeats
#define ECHOBAND_SAFETY_CUTOFF_MS 60000 // 60s hard stop if unacknowledged

#endif // COMMON_PROTOCOL_H
