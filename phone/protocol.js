// protocol.js — Constants and UUIDs mirroring common/protocol.h for EchoBand

export const ECHOBAND_SERVICE_UUID      = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
export const ECHOBAND_CHAR_COMMAND_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
export const ECHOBAND_CHAR_STATUS_UUID  = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

export const ALERT_TYPES = {
  STOP: 0,
  PLATFORM: 1,
  DELAY: 2,
  DEPARTURE: 3,
  URGENT: 4,
  CANCELLED: 5
};

export const ALERT_NAMES = {
  [ALERT_TYPES.STOP]: 'STOP',
  [ALERT_TYPES.PLATFORM]: 'PLATFORM',
  [ALERT_TYPES.DELAY]: 'DELAY',
  [ALERT_TYPES.DEPARTURE]: 'DEPARTURE',
  [ALERT_TYPES.URGENT]: 'URGENT',
  [ALERT_TYPES.CANCELLED]: 'CANCELLED'
};

export const PROFILES = {
  DEAF: 0,
  DEAFBLIND: 1,
  BLIND: 2,
  LANGUAGE: 3
};

export const PROFILE_NAMES = {
  0: 'DEAF',
  1: 'DEAFBLIND',
  2: 'BLIND',
  3: 'LANGUAGE'
};

export const ACK_TIMEOUT_MS = 15000;
