// audio.js — Web Audio capture, loudness gate, and Screen Wake Lock for EchoBand

export class AudioEngine {
  constructor(options = {}) {
    this.onAudioReady = options.onAudioReady || null;
    this.onLevelUpdate = options.onLevelUpdate || null;
    this.onStateChange = options.onStateChange || null;

    this.audioContext = null;
    this.mediaStream = null;
    this.analyser = null;
    this.wakeLock = null;

    this.isListening = false;
    this.isRecording = false;

    // Loudness gate settings
    this.threshold = options.threshold || 0.045; // RMS threshold
    this.minSpeechMs = 300;                      // Must be above threshold for 300ms to start
    this.silenceMs = 700;                        // Stops after 700ms silence
    this.maxDurationMs = 10000;                  // Hard cutoff 10 seconds

    this.aboveThresholdSince = 0;
    this.silenceSince = 0;
    this.recordingStartTime = 0;

    this.recordedPCM = [];
    this.sampleRate = 16000; // Whisper expects 16kHz
    this.scriptProcessor = null;
    this.animFrameId = null;
  }

  async acquireWakeLock() {
    try {
      if ('wakeLock' in navigator) {
        this.wakeLock = await navigator.wakeLock.request('screen');
        console.log('[Audio] Screen Wake Lock acquired');
      }
    } catch (err) {
      console.warn('[Audio] Could not acquire Wake Lock:', err.message);
    }
  }

  releaseWakeLock() {
    if (this.wakeLock) {
      this.wakeLock.release().catch(() => {});
      this.wakeLock = null;
    }
  }

  async startListening() {
    if (this.isListening) return;

    await this.acquireWakeLock();

    // 1. Get user media with DSP constraints
    this.mediaStream = await navigator.mediaDevices.getUserMedia({
      audio: {
        echoCancellation: true,
        noiseSuppression: true,
        autoGainControl: true,
        channelCount: 1
      }
    });

    this.audioContext = new (window.AudioContext || window.webkitAudioContext)({
      sampleRate: this.sampleRate
    });

    const source = this.audioContext.createMediaStreamSource(this.mediaStream);
    this.analyser = this.audioContext.createAnalyser();
    this.analyser.fftSize = 512;
    source.connect(this.analyser);

    // Use ScriptProcessor / AudioWorklet to tap raw PCM
    this.scriptProcessor = this.audioContext.createScriptProcessor(2048, 1, 1);
    source.connect(this.scriptProcessor);
    this.scriptProcessor.connect(this.audioContext.destination);

    this.scriptProcessor.onaudioprocess = (e) => {
      if (this.isRecording) {
        const inputData = e.inputBuffer.getChannelData(0);
        this.recordedPCM.push(new Float32Array(inputData));
      }
    };

    this.isListening = true;
    if (this.onStateChange) this.onStateChange('listening');

    this.monitorLoudness();
    console.log('[Audio] Listening started. Loudness gate active.');
  }

  monitorLoudness() {
    if (!this.isListening) return;

    const dataArray = new Float32Array(this.analyser.fftSize);
    this.analyser.getFloatTimeDomainData(dataArray);

    // Compute RMS
    let sum = 0;
    for (let i = 0; i < dataArray.length; i++) {
      sum += dataArray[i] * dataArray[i];
    }
    const rms = Math.sqrt(sum / dataArray.length);

    if (this.onLevelUpdate) {
      this.onLevelUpdate(rms);
    }

    const now = performance.now();

    if (!this.isRecording) {
      // Check if sound exceeded threshold
      if (rms >= this.threshold) {
        if (this.aboveThresholdSince === 0) {
          this.aboveThresholdSince = now;
        } else if (now - this.aboveThresholdSince >= this.minSpeechMs) {
          // Started speaking!
          this.startRecordingClip();
        }
      } else {
        this.aboveThresholdSince = 0;
      }
    } else {
      // Already recording: check silence or max duration
      if (rms < this.threshold) {
        if (this.silenceSince === 0) {
          this.silenceSince = now;
        } else if (now - this.silenceSince >= this.silenceMs) {
          // Finished speaking
          this.stopRecordingClip();
        }
      } else {
        this.silenceSince = 0;
      }

      // Max duration safeguard
      if (now - this.recordingStartTime >= this.maxDurationMs) {
        this.stopRecordingClip();
      }
    }

    this.animFrameId = requestAnimationFrame(() => this.monitorLoudness());
  }

  startRecordingClip() {
    this.isRecording = true;
    this.recordingStartTime = performance.now();
    this.silenceSince = 0;
    this.recordedPCM = [];
    if (this.onStateChange) this.onStateChange('recording');
    console.log('[Audio] Gate OPEN: Capturing speech clip...');
  }

  stopRecordingClip() {
    if (!this.isRecording) return;
    this.isRecording = false;
    this.aboveThresholdSince = 0;
    this.silenceSince = 0;
    if (this.onStateChange) this.onStateChange('processing');
    console.log('[Audio] Gate CLOSED: Processing clip...');

    // Concatenate PCM buffers into single Float32Array
    let totalLength = 0;
    for (const chunk of this.recordedPCM) {
      totalLength += chunk.length;
    }

    const merged = new Float32Array(totalLength);
    let offset = 0;
    for (const chunk of this.recordedPCM) {
      merged.set(chunk, offset);
      offset += chunk.length;
    }

    this.recordedPCM = [];

    // Only dispatch if clip has reasonable length (> 0.4s)
    if (merged.length > this.sampleRate * 0.4 && this.onAudioReady) {
      this.onAudioReady(merged, this.sampleRate);
    } else {
      if (this.onStateChange) this.onStateChange('listening');
    }
  }

  // Manual Push-to-Listen toggle
  toggleManualCapture() {
    if (!this.isRecording) {
      this.startRecordingClip();
    } else {
      this.stopRecordingClip();
    }
  }

  stopListening() {
    this.isListening = false;
    this.isRecording = false;
    if (this.animFrameId) cancelAnimationFrame(this.animFrameId);
    if (this.scriptProcessor) {
      this.scriptProcessor.disconnect();
      this.scriptProcessor = null;
    }
    if (this.audioContext) {
      this.audioContext.close().catch(() => {});
      this.audioContext = null;
    }
    if (this.mediaStream) {
      this.mediaStream.getTracks().forEach(t => t.stop());
      this.mediaStream = null;
    }
    this.releaseWakeLock();
    if (this.onStateChange) this.onStateChange('idle');
    console.log('[Audio] Listening stopped');
  }
}
