import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  plugins: [svelte()],
  // `host: true` expose le serveur sur le LAN : indispensable pour le live-reload
  // Capacitor sur un iPhone/Android physique (cf. app/README.md).
  server: { host: true, port: 5173 },
  build: { outDir: 'dist', target: 'es2022' },
  test: {
    globals: true,
    environment: 'node',
    include: ['src/**/*.test.ts'],
  },
});
