// asr.worker.js — Web Worker for in-browser multilingual Whisper speech recognition
// Uses @huggingface/transformers (transformers.js) with Xenova/whisper-base

let pipeline = null;
let transcriber = null;
let isLoading = false;

// Dynamic import from CDN inside worker
const TRANSFORMERS_CDN = 'https://cdn.jsdelivr.net/npm/@huggingface/transformers@3.0.2';

async function initModel(language = 'auto') {
  if (transcriber) return transcriber;
  if (isLoading) return;
  isLoading = true;

  try {
    self.postMessage({ status: 'loading', message: 'Importing transformers.js runtime...' });
    const { pipeline: hfPipeline, env } = await import(TRANSFORMERS_CDN);

    // Prefer local cache and WebGPU where possible
    env.allowLocalModels = false;
    env.useBrowserCache = true;

    self.postMessage({ status: 'loading', message: 'Downloading Xenova/whisper-base multilingual model...' });

    // Progress callback for model file downloads
    const progressCallback = (info) => {
      if (info && info.status === 'progress') {
        self.postMessage({
          status: 'progress',
          file: info.file,
          loaded: info.loaded,
          total: info.total,
          progress: Math.round((info.loaded / (info.total || 1)) * 100)
        });
      }
    };

    // Determine device (WebGPU if supported, fallback to wasm)
    let device = 'wasm';
    if (navigator.gpu) {
      try {
        const adapter = await navigator.gpu.requestAdapter();
        if (adapter) device = 'webgpu';
      } catch (e) {
        device = 'wasm';
      }
    }

    self.postMessage({ status: 'loading', message: `Initializing pipeline with device: ${device}...` });

    transcriber = await hfPipeline(
      'automatic-speech-recognition',
      'Xenova/whisper-base',
      {
        device: device,
        progress_callback: progressCallback
      }
    );

    isLoading = false;
    self.postMessage({ status: 'ready', message: 'Whisper-base model loaded and ready!' });
    return transcriber;
  } catch (err) {
    isLoading = false;
    console.error('[ASR Worker] Failed to load whisper model:', err);
    self.postMessage({
      status: 'error',
      message: `Failed to load model: ${err.message}. Check network or use Operator Buttons.`
    });
  }
}

self.onmessage = async (event) => {
  const { type, audio, language, sampleRate } = event.data;

  if (type === 'init') {
    await initModel(language);
    return;
  }

  if (type === 'transcribe') {
    try {
      if (!transcriber) {
        await initModel(language);
      }

      if (!transcriber) {
        self.postMessage({ status: 'error', message: 'Model is not initialized.' });
        return;
      }

      self.postMessage({ status: 'transcribing', message: 'Transcribing speech clip...' });

      const options = {
        chunk_length_s: 30,
        stride_length_s: 5,
        return_timestamps: false
      };

      if (language && language !== 'auto') {
        options.language = language; // 'en', 'ml', 'hi'
      }

      const output = await transcriber(audio, options);
      const text = (output && output.text) ? output.text.trim() : '';

      self.postMessage({ status: 'result', text });
    } catch (err) {
      console.error('[ASR Worker] Transcription error:', err);
      self.postMessage({ status: 'error', message: `Transcription failed: ${err.message}` });
    }
  }
};
