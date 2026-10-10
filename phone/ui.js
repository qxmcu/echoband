// ui.js — Accessible DOM rendering, multilingual cards, and speech synthesis for EchoBand

import { ALERT_TYPES, ALERT_NAMES, ALERT_LABELS } from './match.js';

const PICTOGRAM_MAP = {
  [ALERT_TYPES.PLATFORM]: './assets/platform.svg',
  [ALERT_TYPES.DELAY]: './assets/delay.svg',
  [ALERT_TYPES.DEPARTURE]: './assets/departure.svg',
  [ALERT_TYPES.URGENT]: './assets/urgent.svg',
  [ALERT_TYPES.CANCELLED]: './assets/cancelled.svg'
};

export class UIManager {
  constructor() {
    // DOM Elements
    this.elAlertCard      = document.getElementById('alertCard');
    this.elPictogramImg   = document.getElementById('alertPictogram');
    this.elLabelEn        = document.getElementById('labelEn');
    this.elLabelMl        = document.getElementById('labelMl');
    this.elLabelHi        = document.getElementById('labelHi');
    this.elAlertBadge     = document.getElementById('alertBadge');

    // Live status widgets
    this.elStatMsgId      = document.getElementById('statMsgId');
    this.elStatType       = document.getElementById('statType');
    this.elStatBands      = document.getElementById('statBands');
    this.elStatRx         = document.getElementById('statRx');
    this.elStatAck        = document.getElementById('statAck');
    this.elStatUnconf     = document.getElementById('statUnconf');
    this.elRawStatusLine  = document.getElementById('rawStatusLine');

    // Controls & inputs
    this.elProfileSelect  = document.getElementById('profileSelect');
    this.elTranscriptText = document.getElementById('transcriptText');
    this.elAsrStatus      = document.getElementById('asrStatus');
    this.elAsrProgress    = document.getElementById('asrProgressBar');
    this.elAsrProgressWrap= document.getElementById('asrProgressWrap');

    this.currentProfile = 'deaf';
    this.initProfileListener();
  }

  initProfileListener() {
    if (this.elProfileSelect) {
      this.elProfileSelect.addEventListener('change', (e) => {
        this.currentProfile = e.target.value;
        console.log('[UI] Profile changed to:', this.currentProfile);
      });
    }
  }

  setAsrStatus(text, progress = null) {
    if (this.elAsrStatus) this.elAsrStatus.textContent = text;
    if (progress !== null && this.elAsrProgress) {
      this.elAsrProgressWrap.style.display = 'block';
      this.elAsrProgress.style.width = `${progress}%`;
    } else if (progress === 100) {
      setTimeout(() => {
        if (this.elAsrProgressWrap) this.elAsrProgressWrap.style.display = 'none';
      }, 1000);
    }
  }

  setTranscript(text, isRecognized = true) {
    if (this.elTranscriptText) {
      this.elTranscriptText.textContent = text || 'Listening...';
      if (!isRecognized && text) {
        this.elTranscriptText.textContent += ' (No alert recognized)';
      }
    }
  }

  /**
   * Displays the active alert visually, trilingually, and acoustically if Blind profile.
   */
  displayAlert(type, arg = 0, customLabels = null) {
    if (!type || type === ALERT_TYPES.NONE) {
      this.resetAlert();
      return;
    }

    const labels = customLabels || {
      en: ALERT_LABELS[type].en(arg),
      ml: ALERT_LABELS[type].ml(arg),
      hi: ALERT_LABELS[type].hi(arg)
    };

    // 1. Update Pictogram
    const iconSrc = PICTOGRAM_MAP[type] || './assets/platform.svg';
    if (this.elPictogramImg) {
      this.elPictogramImg.src = iconSrc;
      this.elPictogramImg.alt = labels.en;
    }

    // 2. Update Trilingual Text
    if (this.elLabelEn) this.elLabelEn.textContent = labels.en;
    if (this.elLabelMl) this.elLabelMl.textContent = labels.ml;
    if (this.elLabelHi) this.elLabelHi.textContent = labels.hi;

    // 3. Update Badge
    if (this.elAlertBadge) {
      this.elAlertBadge.textContent = ALERT_NAMES[type] + (arg ? ` (Platform ${arg})` : '');
      this.elAlertBadge.className = `badge badge-${ALERT_NAMES[type].toLowerCase()}`;
    }

    // High visual highlight animation
    if (this.elAlertCard) {
      this.elAlertCard.classList.remove('pulse-urgent');
      if (type === ALERT_TYPES.URGENT) {
        this.elAlertCard.classList.add('pulse-urgent');
      }
    }

    // 4. Blind Profile: Acoustic speech readout
    if (this.currentProfile === 'blind') {
      this.speakAnnouncement(labels.en);
    }
  }

  resetAlert() {
    if (this.elLabelEn) this.elLabelEn.textContent = 'Waiting for announcements';
    if (this.elLabelMl) this.elLabelMl.textContent = 'അറിയിപ്പുകൾക്കായി കാത്തിരിക്കുന്നു';
    if (this.elLabelHi) this.elLabelHi.textContent = 'उद्घोषणा की प्रतीक्षा कर रहे हैं';
    if (this.elAlertBadge) {
      this.elAlertBadge.textContent = 'SYSTEM STANDBY';
      this.elAlertBadge.className = 'badge badge-standby';
    }
    if (this.elAlertCard) this.elAlertCard.classList.remove('pulse-urgent');
  }

  speakAnnouncement(text) {
    if ('speechSynthesis' in window) {
      window.speechSynthesis.cancel(); // Stop prior speech
      const utter = new SpeechSynthesisUtterance(text);
      utter.rate = 1.0;
      utter.pitch = 1.0;
      window.speechSynthesis.speak(utter);
      console.log('[TTS] Speaking:', text);
    }
  }

  /**
   * Parses and updates Station status line:
   * e.g. "msg=12 type=1 arg=2 bands=2 rx=2 ack=1 unconfirmed=A4"
   */
  updateStatusLine(line) {
    if (!line) return;
    if (this.elRawStatusLine) this.elRawStatusLine.textContent = line;

    const parts = line.split(' ');
    const kv = {};
    for (const p of parts) {
      const [k, v] = p.split('=');
      if (k && v !== undefined) kv[k] = v;
    }

    if (kv.msg !== undefined && this.elStatMsgId) this.elStatMsgId.textContent = kv.msg;
    if (kv.type !== undefined && this.elStatType) {
      const tNum = parseInt(kv.type, 10);
      this.elStatType.textContent = ALERT_NAMES[tNum] || 'NONE';
    }
    if (kv.bands !== undefined && this.elStatBands) this.elStatBands.textContent = kv.bands;
    if (kv.rx !== undefined && this.elStatRx) this.elStatRx.textContent = kv.rx;
    if (kv.ack !== undefined && this.elStatAck) this.elStatAck.textContent = kv.ack;

    if (kv.unconfirmed !== undefined && this.elStatUnconf) {
      this.elStatUnconf.textContent = kv.unconfirmed;
      if (kv.unconfirmed === 'NONE') {
        this.elStatUnconf.className = 'status-pill status-pill-success';
      } else {
        this.elStatUnconf.className = 'status-pill status-pill-danger';
      }
    }
  }
}
