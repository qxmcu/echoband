// app.js — Main Application Coordinator for EchoBand Phone Web App (Single-Band Direct BLE)

import { BleTransport, SerialTransport, MockTransport } from './transport.js';
import { AudioEngine } from './audio.js';
import { UIManager } from './ui.js';
import { matchAnnouncement, ALERT_TYPES } from './match.js';

class EchoBandApp {
  constructor() {
    this.ui = new UIManager();
    this.transport = null;
    this.audioEngine = null;
    this.asrWorker = null;

    this.selectedPlatform = 2; // Default Platform 2
    this.currentMsgSeq = 1;
    this.isWorkerReady = false;

    // DOM Bindings
    this.elTransportSelect = document.getElementById('transportType');
    this.elBtnConnect       = document.getElementById('btnConnect');
    this.elBtnListen        = document.getElementById('btnListen');
    this.elAsrLangSelect    = document.getElementById('asrLanguage');
    this.elAudioLevelBar    = document.getElementById('audioLevelBar');
    this.elConnBadge        = document.getElementById('connBadge');

    this.initTransport();
    this.initOperatorButtons();
    this.initAudioEngine();
    this.initASRWorker();
    this.initSpeechSimulators();
    this.initServiceWorker();
  }

  // --- 1. Transport Lifecycle ---
  initTransport() {
    this.setTransportInstance(this.elTransportSelect.value);

    this.elTransportSelect.addEventListener('change', () => {
      if (this.transport && this.transport.isConnected()) {
        this.transport.disconnect();
      }
      this.setTransportInstance(this.elTransportSelect.value);
      this.updateConnectButton(false);
    });

    this.elBtnConnect.addEventListener('click', async () => {
      if (!this.transport.isConnected()) {
        try {
          this.elBtnConnect.disabled = true;
          this.elBtnConnect.textContent = 'Connecting...';
          await this.transport.connect();
          this.updateConnectButton(true);
        } catch (err) {
          alert(`Connection failed: ${err.message}`);
          this.updateConnectButton(false);
        } finally {
          this.elBtnConnect.disabled = false;
        }
      } else {
        await this.transport.disconnect();
        this.updateConnectButton(false);
      }
    });

    // Auto-connect Mock on first load for instant testing!
    if (this.elTransportSelect.value === 'mock') {
      this.transport.connect().then(() => this.updateConnectButton(true)).catch(() => {});
    }
  }

  setTransportInstance(type) {
    if (type === 'ble') {
      this.transport = new BleTransport();
    } else if (type === 'serial') {
      this.transport = new SerialTransport();
    } else {
      this.transport = new MockTransport();
    }

    this.transport.onStatus((line) => {
      this.handleIncomingStatus(line);
    });
  }

  handleIncomingStatus(line) {
    console.log('[App Status]', line);
    if (!line) return;

    if (line.startsWith('RX ')) {
      const msgId = parseInt(line.substring(3).trim(), 10) || 0;
      this.ui.markReceived(msgId);
    } else if (line.startsWith('ACK ')) {
      const msgId = parseInt(line.substring(4).trim(), 10) || 0;
      this.ui.markAcknowledged(msgId);
    } else if (line.startsWith('PROFILE ')) {
      const pName = line.substring(8).trim();
      this.ui.setProfile(pName);
    } else if (line === 'STATUS CONNECTED') {
      if (this.elConnBadge) {
        this.elConnBadge.textContent = 'Connected';
        this.elConnBadge.style.color = '#4ade80';
      }
    } else if (line === 'STATUS DISCONNECTED') {
      if (this.elConnBadge) {
        this.elConnBadge.textContent = 'Disconnected';
        this.elConnBadge.style.color = '#f87171';
      }
    }
  }

  updateConnectButton(connected) {
    if (connected) {
      this.elBtnConnect.textContent = 'Disconnect';
      this.elBtnConnect.className = 'btn-connected';
      if (this.elConnBadge) {
        this.elConnBadge.textContent = 'Connected';
        this.elConnBadge.style.color = '#4ade80';
      }
    } else {
      this.elBtnConnect.textContent = 'Connect';
      this.elBtnConnect.className = 'btn-connect';
      if (this.elConnBadge) {
        this.elConnBadge.textContent = 'Disconnected';
        this.elConnBadge.style.color = '#94a3b8';
      }
    }
  }

  // --- 2. Operator Buttons (Primary Path) ---
  initOperatorButtons() {
    const platBtns = document.querySelectorAll('.btn-pnum');
    platBtns.forEach((btn) => {
      btn.addEventListener('click', () => {
        platBtns.forEach((b) => b.classList.remove('active'));
        btn.classList.add('active');
        this.selectedPlatform = parseInt(btn.getAttribute('data-plat'), 10) || 2;
      });
    });

    document.getElementById('btnOpPlatform').addEventListener('click', () => {
      this.triggerAlert(ALERT_TYPES.PLATFORM, this.selectedPlatform);
    });
    document.getElementById('btnOpDelay').addEventListener('click', () => {
      this.triggerAlert(ALERT_TYPES.DELAY, 0);
    });
    document.getElementById('btnOpDeparture').addEventListener('click', () => {
      this.triggerAlert(ALERT_TYPES.DEPARTURE, 0);
    });
    document.getElementById('btnOpUrgent').addEventListener('click', () => {
      this.triggerAlert(ALERT_TYPES.URGENT, 0);
    });
    document.getElementById('btnOpCancelled').addEventListener('click', () => {
      this.triggerAlert(ALERT_TYPES.CANCELLED, 0);
    });
  }

  async triggerAlert(type, arg = 0, labels = null) {
    this.currentMsgSeq = (this.currentMsgSeq + 1) % 255;
    if (this.currentMsgSeq === 0) this.currentMsgSeq = 1;
    const msgId = this.currentMsgSeq;

    this.ui.displayAlert(type, arg, labels);

    if (this.transport && this.transport.isConnected()) {
      try {
        await this.transport.send(type, arg, msgId);
      } catch (err) {
        console.error('[App] Transport send error:', err);
      }
    } else {
      console.warn('[App] Transport is not connected.');
    }
  }

  // --- 3. Audio & Speech Engine (Native Web Speech + Whisper Fallback) ---
  initAudioEngine() {
    this.nativeRecognizer = null;
    const SpeechRec = window.SpeechRecognition || window.webkitSpeechRecognition;

    if (SpeechRec) {
      this.nativeRecognizer = new SpeechRec();
      this.nativeRecognizer.continuous = false;
      this.nativeRecognizer.interimResults = false;

      this.nativeRecognizer.onstart = () => {
        this.elBtnListen.textContent = 'Stop Mic';
        this.elBtnListen.classList.add('listening');
        this.ui.setAsrStatus('Listening (Native Speech)...');
      };

      this.nativeRecognizer.onresult = (event) => {
        const text = event.results[0][0].transcript;
        console.log('[Native Speech] Result:', text);
        this.handleTranscript(text);
      };

      this.nativeRecognizer.onerror = (event) => {
        console.warn('[Native Speech] Error:', event.error);
        this.ui.setAsrStatus(`Mic error: ${event.error}`);
        this.stopListeningUI();
      };

      this.nativeRecognizer.onend = () => {
        this.stopListeningUI();
      };
    }

    this.audioEngine = new AudioEngine({
      onLevelUpdate: (rms) => {
        const pct = Math.min(100, Math.round(rms * 400));
        if (this.elAudioLevelBar) {
          this.elAudioLevelBar.style.width = `${pct}%`;
        }
      },
      onStateChange: (state) => {
        if (state === 'listening') {
          this.elBtnListen.textContent = 'Stop Mic';
          this.elBtnListen.classList.add('listening');
          this.ui.setAsrStatus('Listening (Whisper Gate)...');
        } else if (state === 'recording') {
          this.ui.setAsrStatus('Speech detected! Recording...');
        } else if (state === 'processing') {
          this.ui.setAsrStatus('Processing audio clip...');
        } else {
          this.stopListeningUI();
        }
      },
      onAudioReady: (audioPCM, sampleRate) => {
        this.processSpeechClip(audioPCM, sampleRate);
      }
    });

    this.elBtnListen.addEventListener('click', async () => {
      const engine = document.getElementById('asrEngine')?.value || 'native';

      if (this.elBtnListen.classList.contains('listening')) {
        if (this.nativeRecognizer) this.nativeRecognizer.stop();
        if (this.audioEngine) this.audioEngine.stopListening();
        this.stopListeningUI();
        return;
      }

      if (engine === 'native' && this.nativeRecognizer) {
        const langMap = { en: 'en-IN', ml: 'ml-IN', hi: 'hi-IN', auto: 'en-IN' };
        this.nativeRecognizer.lang = langMap[this.elAsrLangSelect.value] || 'en-IN';
        try {
          this.nativeRecognizer.start();
        } catch (err) {
          console.warn('[Native Speech] Start failed:', err);
        }
      } else {
        try {
          await this.audioEngine.startListening();
        } catch (err) {
          alert(`Microphone access error: ${err.message}`);
        }
      }
    });
  }

  stopListeningUI() {
    this.elBtnListen.textContent = 'Listen Mic';
    this.elBtnListen.classList.remove('listening');
    this.ui.setAsrStatus('Standby');
    if (this.elAudioLevelBar) this.elAudioLevelBar.style.width = '0%';
  }

  // --- 4. Whisper Speech Recognition Worker ---
  initASRWorker() {
    try {
      this.asrWorker = new Worker('./asr.worker.js', { type: 'module' });

      this.asrWorker.onmessage = (e) => {
        const data = e.data;
        if (data.status === 'loading') {
          this.ui.setAsrStatus(data.message);
        } else if (data.status === 'progress') {
          this.ui.setAsrStatus(`Downloading Whisper base: ${data.progress}%`, data.progress);
        } else if (data.status === 'ready') {
          this.isWorkerReady = true;
          this.ui.setAsrStatus('Whisper Model Ready', 100);
        } else if (data.status === 'transcribing') {
          this.ui.setAsrStatus('Transcribing speech...');
        } else if (data.status === 'result') {
          this.handleTranscript(data.text);
        } else if (data.status === 'error') {
          this.ui.setAsrStatus(data.message);
        }
      };

      this.asrWorker.postMessage({
        type: 'init',
        language: this.elAsrLangSelect.value
      });
    } catch (err) {
      console.warn('[App] Web Worker initialization error:', err);
      this.ui.setAsrStatus('ASR Worker unavailable (Use Native Speech or Operator Buttons)');
    }
  }

  processSpeechClip(audioPCM, sampleRate) {
    if (!this.asrWorker) return;
    this.asrWorker.postMessage({
      type: 'transcribe',
      audio: audioPCM,
      language: this.elAsrLangSelect.value,
      sampleRate: sampleRate
    });
  }

  // --- 5. Quick Speech Simulators & Judges Demo Helper ---
  initSpeechSimulators() {
    const chips = document.querySelectorAll('.btn-chip');
    chips.forEach((chip) => {
      chip.addEventListener('click', () => {
        const phrase = chip.getAttribute('data-phrase');
        if (phrase) this.handleTranscript(phrase);
      });
    });

    const input = document.getElementById('manualSpeechInput');
    const parseBtn = document.getElementById('btnSimulateSpeech');
    if (parseBtn && input) {
      parseBtn.addEventListener('click', () => {
        const text = input.value.trim();
        if (text) {
          this.handleTranscript(text);
          input.value = '';
        }
      });
      input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
          parseBtn.click();
        }
      });
    }
  }

  handleTranscript(text) {
    console.log('[App] Transcript:', text);
    const match = matchAnnouncement(text);

    if (match.matched) {
      this.ui.setTranscript(`"${text}" -> [${match.typeName}]`, true);
      this.triggerAlert(match.type, match.arg, match.labels);
    } else {
      this.ui.setTranscript(`"${text}"`, false);
    }
  }

  // --- 6. Service Worker Offline Support ---
  initServiceWorker() {
    if ('serviceWorker' in navigator) {
      navigator.serviceWorker.register('./sw.js')
        .then((reg) => console.log('[SW] Service Worker registered:', reg.scope))
        .catch((err) => console.warn('[SW] Registration failed:', err));
    }
  }
}

// Instantiate on DOM ready
window.addEventListener('DOMContentLoaded', () => {
  window.echoApp = new EchoBandApp();
});
