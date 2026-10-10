// transport.js — Unified communication transport layer for EchoBand
// Direct connection to EchoBand over Web Bluetooth, Web Serial, and Mock Simulator

import {
  ECHOBAND_SERVICE_UUID,
  ECHOBAND_CHAR_COMMAND_UUID,
  ECHOBAND_CHAR_STATUS_UUID
} from './protocol.js';

export class BaseTransport {
  constructor() {
    this.statusListeners = [];
    this.connected = false;
  }

  onStatus(callback) {
    if (typeof callback === 'function') {
      this.statusListeners.push(callback);
    }
  }

  notifyStatus(line) {
    for (const listener of this.statusListeners) {
      try {
        listener(line);
      } catch (err) {
        console.error('[Transport] Listener error:', err);
      }
    }
  }

  isConnected() {
    return this.connected;
  }

  async connect() { throw new Error('Not implemented'); }
  async disconnect() { throw new Error('Not implemented'); }
  async send(type, arg, msgId) { throw new Error('Not implemented'); }
}

/**
 * Web Bluetooth Transport (Android Chrome directly to EchoBand)
 */
export class BleTransport extends BaseTransport {
  constructor() {
    super();
    this.device = null;
    this.server = null;
    this.cmdChar = null;
    this.statusChar = null;
  }

  async connect() {
    if (!navigator.bluetooth) {
      throw new Error('Web Bluetooth is not supported on this browser. Use Chrome on Android over HTTPS.');
    }

    console.log('[BLE] Requesting EchoBand Bluetooth Device...');
    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ name: 'EchoBand' }],
      optionalServices: [ECHOBAND_SERVICE_UUID]
    });

    this.device.addEventListener('gattserverdisconnected', () => {
      this.connected = false;
      this.notifyStatus('STATUS DISCONNECTED');
      console.log('[BLE] EchoBand disconnected');
    });

    console.log('[BLE] Connecting to GATT Server...');
    this.server = await this.device.gatt.connect();

    console.log('[BLE] Getting EchoBand Service...');
    const service = await this.server.getPrimaryService(ECHOBAND_SERVICE_UUID);

    console.log('[BLE] Getting Characteristics...');
    this.cmdChar = await service.getCharacteristic(ECHOBAND_CHAR_COMMAND_UUID);
    this.statusChar = await service.getCharacteristic(ECHOBAND_CHAR_STATUS_UUID);

    // Subscribe to status notifications
    await this.statusChar.startNotifications();
    this.statusChar.addEventListener('characteristicvaluechanged', (event) => {
      const decoder = new TextDecoder('utf-8');
      const line = decoder.decode(event.target.value).trim();
      console.log('[BLE RX Notify]', line);
      this.notifyStatus(line);
    });

    this.connected = true;
    console.log('[BLE] Connected directly to EchoBand!');
    this.notifyStatus('STATUS CONNECTED');
    return true;
  }

  async disconnect() {
    if (this.device && this.device.gatt.connected) {
      await this.device.gatt.disconnect();
    }
    this.connected = false;
    this.notifyStatus('STATUS DISCONNECTED');
  }

  async send(type, arg = 0, msgId = 1) {
    if (!this.connected || !this.cmdChar) {
      throw new Error('EchoBand is not connected.');
    }
    // 3 bytes [type, arg, msgId]
    const payload = new Uint8Array([type & 0xFF, arg & 0xFF, msgId & 0xFF]);
    await this.cmdChar.writeValue(payload);
    console.log(`[BLE TX] Sent [type=${type}, arg=${arg}, msgId=${msgId}]`);
  }
}

/**
 * Web Serial Transport (Laptop USB Chrome fallback)
 */
export class SerialTransport extends BaseTransport {
  constructor() {
    super();
    this.port = null;
    this.writer = null;
    this.reader = null;
    this.keepReading = false;
  }

  async connect() {
    if (!navigator.serial) {
      throw new Error('Web Serial is not supported on this browser. Use Chrome on desktop.');
    }

    console.log('[Serial] Requesting Serial Port...');
    this.port = await navigator.serial.requestPort();
    await this.port.open({ baudRate: 115200 });

    const textEncoder = new TextEncoderStream();
    textEncoder.readable.pipeTo(this.port.writable);
    this.writer = textEncoder.writable.getWriter();

    this.connected = true;
    this.keepReading = true;
    this.readLoop();
    console.log('[Serial] Connected to EchoBand at 115200 baud');
    this.notifyStatus('STATUS CONNECTED');
    return true;
  }

  async readLoop() {
    const textDecoder = new TextDecoderStream();
    this.port.readable.pipeTo(textDecoder.writable);
    this.reader = textDecoder.readable.getReader();

    let buffer = '';
    try {
      while (this.keepReading) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (value) {
          buffer += value;
          const lines = buffer.split('\n');
          buffer = lines.pop();
          for (const raw of lines) {
            const line = raw.trim();
            if (line.length > 0) {
              if (line.startsWith('RX ') || line.startsWith('ACK ') || line.startsWith('PROFILE ')) {
                this.notifyStatus(line);
              }
            }
          }
        }
      }
    } catch (err) {
      console.warn('[Serial] Reader ended:', err);
    } finally {
      this.connected = false;
      this.notifyStatus('STATUS DISCONNECTED');
    }
  }

  async disconnect() {
    this.keepReading = false;
    if (this.reader) await this.reader.cancel();
    if (this.writer) await this.writer.close();
    if (this.port) await this.port.close();
    this.connected = false;
    this.notifyStatus('STATUS DISCONNECTED');
  }

  async send(type, arg = 0, msgId = 1) {
    if (!this.connected || !this.writer) {
      throw new Error('Serial Transport is not connected.');
    }
    const cmd = `A ${type} ${arg} ${msgId}\n`;
    await this.writer.write(cmd);
    console.log(`[Serial TX] Sent: ${cmd.trim()}`);
  }
}

/**
 * Mock Transport (Zero hardware simulator)
 */
export class MockTransport extends BaseTransport {
  constructor() {
    super();
    this.mockAckTimer = null;
  }

  async connect() {
    this.connected = true;
    console.log('[Mock] Connected (Simulating EchoBand wearable)');
    this.notifyStatus('STATUS CONNECTED');
    this.notifyStatus('PROFILE DEAF');
    return true;
  }

  async disconnect() {
    this.connected = false;
    if (this.mockAckTimer) clearTimeout(this.mockAckTimer);
    console.log('[Mock] Disconnected');
    this.notifyStatus('STATUS DISCONNECTED');
  }

  async send(type, arg = 0, msgId = 1) {
    if (!this.connected) throw new Error('Mock Transport is disconnected.');

    console.log(`[Mock TX] Sent [type=${type}, arg=${arg}, msgId=${msgId}]`);

    // 1. Instantly simulate Band reception
    this.notifyStatus(`RX ${msgId}`);

    // 2. After 3.5s (sensory wave completes)
    if (this.mockAckTimer) clearTimeout(this.mockAckTimer);
    this.mockAckTimer = setTimeout(() => {
      if (this.connected) {
        console.log(`[Mock] Alert pattern completed.`);
        this.notifyStatus(`ACK ${msgId}`);
      }
    }, 3500);
  }
}
