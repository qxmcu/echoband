// transport.js — Unified communication transport layer for EchoBand
// Supports: Web Bluetooth (Phone Chrome), Web Serial (Laptop Chrome), and Mock (Offline simulation)

const BLE_SERVICE_UUID     = '0000ec00-0000-1000-8000-00805f9b34fb';
const BLE_CHAR_WRITE_UUID  = '0000ec01-0000-1000-8000-00805f9b34fb';
const BLE_CHAR_NOTIFY_UUID = '0000ec02-0000-1000-8000-00805f9b34fb';

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
  async send(type, arg) { throw new Error('Not implemented'); }
}

/**
 * Web Bluetooth Transport (Android Chrome)
 */
export class BleTransport extends BaseTransport {
  constructor() {
    super();
    this.device = null;
    this.server = null;
    this.writeChar = null;
    this.notifyChar = null;
  }

  async connect() {
    if (!navigator.bluetooth) {
      throw new Error('Web Bluetooth is not supported on this browser (Use Chrome on Android).');
    }

    console.log('[BLE] Requesting Bluetooth Device...');
    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ name: 'EchoStation' }],
      optionalServices: [BLE_SERVICE_UUID]
    });

    this.device.addEventListener('gattserverdisconnected', () => {
      this.connected = false;
      this.notifyStatus('msg=0 type=0 arg=0 bands=0 rx=0 ack=0 unconfirmed=DISCONNECTED');
      console.log('[BLE] Device disconnected');
    });

    console.log('[BLE] Connecting to GATT Server...');
    this.server = await this.device.gatt.connect();

    console.log('[BLE] Getting Service...');
    const service = await this.server.getPrimaryService(BLE_SERVICE_UUID);

    console.log('[BLE] Getting Characteristics...');
    this.writeChar = await service.getCharacteristic(BLE_CHAR_WRITE_UUID);
    this.notifyChar = await service.getCharacteristic(BLE_CHAR_NOTIFY_UUID);

    // Subscribe to status notifications
    await this.notifyChar.startNotifications();
    this.notifyChar.addEventListener('characteristicvaluechanged', (event) => {
      const decoder = new TextDecoder('utf-8');
      const line = decoder.decode(event.target.value);
      this.notifyStatus(line.trim());
    });

    this.connected = true;
    console.log('[BLE] Connected successfully to EchoStation');
    return true;
  }

  async disconnect() {
    if (this.device && this.device.gatt.connected) {
      await this.device.gatt.disconnect();
    }
    this.connected = false;
  }

  async send(type, arg = 0) {
    if (!this.connected || !this.writeChar) {
      throw new Error('BLE Transport is not connected.');
    }
    const payload = new Uint8Array([type, arg]);
    await this.writeChar.writeValue(payload);
    console.log(`[BLE TX] Sent alert type=${type}, arg=${arg}`);
  }
}

/**
 * Web Serial Transport (Laptop Chrome fallback)
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
      throw new Error('Web Serial is not supported on this browser (Use Chrome on desktop).');
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
    console.log('[Serial] Connected to ESP32 Serial at 115200 baud');
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
          buffer = lines.pop(); // Keep partial line
          for (const raw of lines) {
            const line = raw.trim();
            if (line.startsWith('msg=')) {
              this.notifyStatus(line);
            }
          }
        }
      }
    } catch (err) {
      console.warn('[Serial] Reader ended:', err);
    } finally {
      this.connected = false;
    }
  }

  async disconnect() {
    this.keepReading = false;
    if (this.reader) await this.reader.cancel();
    if (this.writer) await this.writer.close();
    if (this.port) await this.port.close();
    this.connected = false;
  }

  async send(type, arg = 0) {
    if (!this.connected || !this.writer) {
      throw new Error('Serial Transport is not connected.');
    }
    const cmd = `A ${type} ${arg}\n`;
    await this.writer.write(cmd);
    console.log(`[Serial TX] Sent command: ${cmd.trim()}`);
  }
}

/**
 * Mock Transport (Zero hardware required — test in any browser)
 */
export class MockTransport extends BaseTransport {
  constructor() {
    super();
    this.mockMsgId = 0;
    this.mockAckTimer = null;
  }

  async connect() {
    this.connected = true;
    console.log('[Mock] Connected in Simulator Mode (Simulating Station & Band 0x4A)');
    // Initial status
    this.notifyStatus('msg=0 type=0 arg=0 bands=1 rx=0 ack=0 unconfirmed=NONE');
    return true;
  }

  async disconnect() {
    this.connected = false;
    if (this.mockAckTimer) clearTimeout(this.mockAckTimer);
    console.log('[Mock] Disconnected');
  }

  async send(type, arg = 0) {
    if (!this.connected) throw new Error('Mock Transport is disconnected.');

    this.mockMsgId++;
    const currentId = this.mockMsgId;

    console.log(`[Mock TX] Alert sent -> msg=${currentId} type=${type} arg=${arg}`);

    // 1. Immediately simulate Station broadcast & Band reception
    this.notifyStatus(`msg=${currentId} type=${type} arg=${arg} bands=1 rx=1 ack=0 unconfirmed=4A`);

    // 2. After 3.5 seconds (wearer feels wave & double-taps), simulate Double-Tap ACK
    if (this.mockAckTimer) clearTimeout(this.mockAckTimer);
    this.mockAckTimer = setTimeout(() => {
      if (this.connected && this.mockMsgId === currentId) {
        console.log(`[Mock Band] Double-tap detected on Band 0x4A! Acknowledging msg=${currentId}...`);
        this.notifyStatus(`msg=${currentId} type=${type} arg=${arg} bands=1 rx=1 ack=1 unconfirmed=NONE`);
      }
    }, 3500);
  }
}
