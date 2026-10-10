// sw.js — EchoBand Service Worker (Network-first to guarantee latest live updates)

const CACHE_NAME = 'echoband-cache-v2';

const STATIC_ASSETS = [
  './',
  './index.html',
  './app.js',
  './ui.js',
  './transport.js',
  './audio.js',
  './match.js',
  './asr.worker.js',
  './assets/platform.svg',
  './assets/delay.svg',
  './assets/departure.svg',
  './assets/urgent.svg',
  './assets/cancelled.svg'
];

self.addEventListener('install', (event) => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) => {
      return Promise.all(keys.map((key) => caches.delete(key)));
    })
  );
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  // Network-first strategy: always fetch live changes, only fallback to cache if completely offline
  event.respondWith(
    fetch(event.request)
      .then((networkResponse) => {
        if (event.request.method === 'GET' && networkResponse && networkResponse.status === 200) {
          const responseToCache = networkResponse.clone();
          caches.open(CACHE_NAME).then((cache) => cache.put(event.request, responseToCache));
        }
        return networkResponse;
      })
      .catch(() => caches.match(event.request))
  );
});
