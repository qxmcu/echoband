// sw.js — EchoBand Service Worker for offline PWA caching

const CACHE_NAME = 'echoband-cache-v1';

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
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      console.log('[SW] Caching static app shell & pictograms...');
      return cache.addAll(STATIC_ASSETS);
    })
  );
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) => {
      return Promise.all(
        keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))
      );
    })
  );
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  // Cache-first for local static assets, network-first for external CDN
  event.respondWith(
    caches.match(event.request).then((cachedResponse) => {
      if (cachedResponse) {
        return cachedResponse;
      }
      return fetch(event.request).then((networkResponse) => {
        // Cache successful GET requests
        if (event.request.method === 'GET' && networkResponse && networkResponse.status === 200) {
          const responseToCache = networkResponse.clone();
          caches.open(CACHE_NAME).then((cache) => {
            cache.put(event.request, responseToCache);
          });
        }
        return networkResponse;
      }).catch(() => {
        // Fallback for offline if not in cache
        return caches.match('./index.html');
      });
    })
  );
});
