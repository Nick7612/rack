// Cache the whole game so it keeps working with no network after the first
// visit. Bump CACHE when shipping new assets.
const CACHE = 'rack-v1';

const ASSETS = [
    './',
    './index.html',
    './css/app.css',
    './js/main.js',
    './js/game.js',
    './js/physics.js',
    './js/render.js',
    './js/audio.js',
    './sounds/click.wav',
    './sounds/rail.wav',
    './sounds/pocket.wav',
    './icon.svg',
    './manifest.webmanifest',
];

self.addEventListener('install', (event) => {
    event.waitUntil(
        caches.open(CACHE)
            .then((cache) => cache.addAll(ASSETS))
            .then(() => self.skipWaiting()),
    );
});

self.addEventListener('activate', (event) => {
    event.waitUntil(
        caches.keys()
            .then((keys) => Promise.all(keys
                .filter((key) => key !== CACHE)
                .map((key) => caches.delete(key))))
            .then(() => self.clients.claim()),
    );
});

self.addEventListener('fetch', (event) => {
    if (event.request.method !== 'GET')
        return;
    const url = new URL(event.request.url);
    if (url.origin !== self.location.origin)
        return;

    event.respondWith(
        caches.match(event.request).then((cached) => cached || fetch(event.request)
            .then((response) => {
                const copy = response.clone();
                caches.open(CACHE).then((cache) => cache.put(event.request, copy));
                return response;
            })
            .catch(() => caches.match('./index.html'))),
    );
});
