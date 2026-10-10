// ui.js — Universal DOM rendering, multimodal cards, timeline, and speech synthesis for EchoBand

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

    // Timeline elements
    this.elTimelineSent   = document.getElementById('tlSent');
    this.elTimelineRx     = document.getElementById('tlRx');
    this.elTimelineAck    = document.getElementById('tlAck');
    this.elTimelineMsg    = document.getElementById('tlStatusMsg');

    // Profile & Transcript
    this.elProfileSelect  = document.getElementById('profileSelect');
    this.elTranscriptText = document.getElementById('transcriptText');
    this.elAsrStatus      = document.getElementById('asrStatus');
    this.elAsrProgress    = document.getElementById('asrProgressBar');
    this.elAsrProgressWrap= document.getElementById('asrProgressWrap');

    this.currentProfile = 'deaf';
    this.ackTimer = null;
    this.initProfileListener();
  }

  initProfileListener() {
    if (this.elProfileSelect) {
      this.elProfileSelect.addEventListener('change', (e) => {
        this.setProfile(e.target.value.toLowerCase(), false);
      });
    }
  }

  setProfile(profileName, updateDropdown = true) {
    const clean = profileName.toLowerCase();
    this.currentProfile = clean;
    if (updateDropdown && this.elProfileSelect) {
      this.elProfileSelect.value = clean;
    }
    console.log('[UI] Profile active:', this.currentProfile);
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
      this.elTranscriptText.textContent = text || 'Listening for speech...';
      if (!isRecognized && text) {
        this.elTranscriptText.textContent += ' (No alert recognized)';
      }
    }
  }

  /**
   * Displays the active alert visually, trilingually, and acoustically.
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

    // Update Pictogram
    const iconSrc = PICTOGRAM_MAP[type] || './assets/platform.svg';
    if (this.elPictogramImg) {
      this.elPictogramImg.src = iconSrc;
      this.elPictogramImg.alt = labels.en;
    }

    // Update Trilingual Text
    if (this.elLabelEn) this.elLabelEn.textContent = labels.en;
    if (this.elLabelMl) this.elLabelMl.textContent = labels.ml;
    if (this.elLabelHi) this.elLabelHi.textContent = labels.hi;

    // Update Badge
    const nameMap = {
      [ALERT_TYPES.PLATFORM]: 'location',
      [ALERT_TYPES.DELAY]: 'delay',
      [ALERT_TYPES.DEPARTURE]: 'departure',
      [ALERT_TYPES.URGENT]: 'urgent',
      [ALERT_TYPES.CANCELLED]: 'cancelled'
    };
    const styleKey = nameMap[type] || 'standby';

    if (this.elAlertBadge) {
      this.elAlertBadge.textContent = ALERT_NAMES[type] + (arg ? ` ${arg}` : '');
      this.elAlertBadge.className = `badge-pill badge-${styleKey}`;
    }

    // High visual highlight
    if (this.elAlertCard) {
      this.elAlertCard.classList.remove('urgent-mode');
      if (type === ALERT_TYPES.URGENT) {
        this.elAlertCard.classList.add('urgent-mode');
      }
    }

    // Blind Profile: Acoustic speech readout
    if (this.currentProfile === 'blind') {
      this.speakAnnouncement(labels.en);
    }

    // Timeline: Step 1 (SENT)
    this.startTimeline();
  }

  resetAlert() {
    if (this.elLabelEn) this.elLabelEn.textContent = 'Waiting for announcement';
    if (this.elLabelMl) this.elLabelMl.textContent = 'അറിയിപ്പുകൾക്കായി കാത്തിരിക്കുന്നു';
    if (this.elLabelHi) this.elLabelHi.textContent = 'उद्घोषणा की प्रतीक्षा कर रहे हैं';
    if (this.elAlertBadge) {
      this.elAlertBadge.textContent = 'SYSTEM STANDBY';
      this.elAlertBadge.className = 'badge-pill badge-standby';
    }
    if (this.elAlertCard) this.elAlertCard.classList.remove('urgent-mode');
  }

  // --- Alert Timeline Flow (SENT -> DELIVERED -> ACKNOWLEDGED / NOT CONFIRMED) ---
  startTimeline() {
    if (this.ackTimer) clearTimeout(this.ackTimer);

    if (this.elTimelineSent) this.elTimelineSent.className = 'tl-node active';
    if (this.elTimelineRx)   this.elTimelineRx.className   = 'tl-node';
    if (this.elTimelineAck)  this.elTimelineAck.className  = 'tl-node';
    if (this.elTimelineMsg) {
      this.elTimelineMsg.textContent = 'Broadcasting signal to EchoBand...';
      this.elTimelineMsg.style.color = '#94a3b8';
    }

    // 15s unconfirmed countdown
    this.ackTimer = setTimeout(() => {
      if (this.elTimelineAck && !this.elTimelineAck.classList.contains('success')) {
        this.elTimelineAck.className = 'tl-node failed';
        if (this.elTimelineMsg) {
          this.elTimelineMsg.textContent = '⚠️ NOT CONFIRMED after 15 s (Wearer has not double-tapped)';
          this.elTimelineMsg.style.color = '#f87171';
        }
      }
    }, 15000);
  }

  markReceived(msgId) {
    if (this.elTimelineRx)  this.elTimelineRx.className = 'tl-node active';
    if (this.elTimelineMsg) {
      this.elTimelineMsg.textContent = `EchoBand playing sensory wave (Msg #${msgId})...`;
      this.elTimelineMsg.style.color = '#38bdf8';
    }
  }

  markAcknowledged(msgId) {
    if (this.ackTimer) clearTimeout(this.ackTimer);
    if (this.elTimelineAck) this.elTimelineAck.className = 'tl-node success';
    if (this.elTimelineMsg) {
      this.elTimelineMsg.textContent = `✅ ACKNOWLEDGED: Double-tap confirmed on wrist (Msg #${msgId})!`;
      this.elTimelineMsg.style.color = '#4ade80';
    }
  }

  speakAnnouncement(text) {
    if ('speechSynthesis' in window) {
      window.speechSynthesis.cancel();
      const utter = new SpeechSynthesisUtterance(text);
      utter.rate = 1.0;
      utter.pitch = 1.0;
      window.speechSynthesis.speak(utter);
      console.log('[TTS] Speaking:', text);
    }
  }
}
