---
type: architecture
tags: [architecture, ui, svelte, capacitor, prototyping, stack]
created: 2026-06-02
updated: 2026-06-02
status: draft
---

# Stack technique Svelte (prototype Capacitor)

Recette concrète de l'**Option 5** de [[cross-platform-app-stack]] (Web + Capacitor, iPhone-first, RNode direct). Choix : **Svelte 5**.

## Stack

| Couche | Choix | Pourquoi |
|---|---|---|
| **Base** | **Vite + Svelte 5 (runes) + TypeScript** | SPA pure (pas de SSR, app locale) ; TS pour le protocole binaire |
| *(opt.) routing* | SvelteKit + `adapter-static` (SPA, `ssr=false`) | seulement si file-based routing voulu |
| **Shell natif** | **Capacitor 7** | iOS + Android depuis la base web |
| **BLE** | **`@capacitor-community/bluetooth-le`** | natif iOS/Android + fallback Web Bluetooth (dev desktop) |
| **Carte** | **MapLibre GL JS** | open-source, sans token |
| **Cartes offline** | **PMTiles** (protomaps) | archive mono-fichier lue par MapLibre, zéro serveur |
| *(opt.) wrapper carte* | `svelte-maplibre` | composants déclaratifs ; sinon MapLibre brut dans `$effect` |
| **GPS** | `@capacitor/geolocation` (proto) → **GPS T-Beam via BLE** (produit) | démarre au GPS du tel, bascule u-blox M10S ensuite |
| **Stockage** | Capacitor **Preferences** (config) + **Dexie/IndexedDB** ou `@capacitor-community/sqlite` | offline persistant (historique) |
| **État** | **runes** (`$state`/`$derived`) dans `.svelte.ts` | position propre, flotte, messages |
| **Protocole** | **module TS maison** : KISS + trames tactiques, sur l'**UART BLE du [[rnode]]** | `Uint8Array`/`DataView` |
| **Style** | CSS scoped Svelte (+ Tailwind opt.) | HUD tactique sur-mesure |
| **Icônes** | `lucide-svelte` | |
| **Tests** | **Vitest** | encode/decode du protocole = test unitaire idéal |

## Workflow dev (iPhone-first)
```
npm create vite@latest airsoft-app -- --template svelte-ts
npm i @capacitor/core @capacitor/cli && npx cap init
npm i @capacitor-community/bluetooth-le @capacitor/geolocation maplibre-gl pmtiles
npx cap add ios && npx cap add android
```
1. **Dev Chrome desktop** : plugin BLE → fallback Web Bluetooth → itérer l'UI sans build natif.
2. **Test iPhone** : `npx cap run ios` + **live-reload** (`server.url` = Vite sur le LAN).
3. **Xcode** pour signer/déployer sur son iPhone (compte Apple gratuit = 7 j).

## Structure
```
src/lib/
  ble/   rnode.ts        ← connexion BLE + framing KISS
  proto/ frames.ts       ← encode/decode trames tactiques (TS)
  state/ fleet.svelte.ts ← runes : maPosition, flotte[], messages
  map/   Map.svelte      ← MapLibre + PMTiles + marqueurs
```

## Notes de conception
1. **Source GPS** : démarrer au **GPS du téléphone** (`@capacitor/geolocation`, zéro friction) ; basculer ensuite sur le **u-blox M10S du [[lilygo-t-beam]]** via le **même lien BLE** que les messages (câblé dans `rnode.ts`).
2. **Cœur = `proto/frames.ts`** : le **mini-protocole tactique** (position, touché/éliminé, waypoint, ordre) en binaire compact sur KISS. **Première brique à écrire + tester** (Vitest) ; le reste est de l'UI autour. Lien : [[lxmf]] (ce que ce protocole remplace temporairement), [[from-scratch-feasibility]].

Liens : [[cross-platform-app-stack]], [[phone-app-architecture]], [[rnode]]
Source : [[2026-06-01_sessions-conception]] · capacitorjs.com, maplibre.org, protomaps PMTiles
