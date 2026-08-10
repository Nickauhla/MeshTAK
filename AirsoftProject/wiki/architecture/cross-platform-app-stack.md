---
type: architecture
tags: [architecture, ui, cross-platform, reticulum, prototyping]
created: 2026-06-02
updated: 2026-06-02
status: draft
---

# Pile applicative multiplateforme (un seul code, Win/Android/Apple)

Question : **une seule base de code** pour Windows + Android + Apple, utilisant [[reticulum]], pour **prototyper vite**. Détaille le « chemin C » de [[phone-app-architecture]].

## Contrainte de fond
[[reticulum]] est né en **Python** (RNS, seule implémentation complète). En **2026**, des ports existent — **Rust** (Reticulum-rs), **Go** (go-reticulum), **C++** (microReticulum), **Zig**, tentative **ReticulumiOS** — mais **jeunes, parité non garantie**. FOSDEM 2026 : Mark Qvist se retire → écosystème communautaire multi-implémentations. ⇒ **prototyper sur le Python éprouvé**, surveiller les cœurs natifs pour le **produit**.

Mécanisme clé : **`rnsd`** (démon RNS) tourne en tâche de fond et **expose l'instance** à d'autres programmes (clé RPC partagée, y compris Android). C'est ce qui permet à une **UI non-Python** de parler à Reticulum sans réécrire la pile.

## Options

| Option | Langage / UI | Reticulum | Win | Android | Apple | Proto |
|---|---|---|---|---|---|---|
| **1. Fork [[sideband]]** | Python + **Kivy** | **embarqué** | ✅ | ✅ | macOS ✅ / **iOS dur** | ⭐⭐⭐ |
| **2. BeeWare/Toga** | Python natif | **embarqué** | ✅ | ✅ | macOS ✅ / **iOS visé** | ⭐⭐ |
| **3. Flutter + `rnsd`** | Dart | **démon (RPC)** | ✅ | ✅ (Chaquopy/Termux) | macOS ✅ / **iOS = dur** | ⭐ |
| **4. Cœur Rust/Go + UI native** | Rust/Go + Flutter/Tauri | **compilé natif** | ✅ | ✅ | ✅ théorique | ⚠️ immature |
| **5. Web + Capacitor** | HTML/JS (React/Vue/Svelte) | **❌ pas dans le navigateur** → **RNode direct** | ✅ | ✅ | **iOS ✅** + macOS | ⭐⭐⭐ (si iPhone-first) |

## Option 5 — Web + Capacitor (la meilleure si debug iPhone-first)
Cas d'usage : **un seul iPhone pour débugger** + portage Android simple + offline + parler au T-Beam.

**Le mur PWA** : une PWA parle au BLE via **Web Bluetooth** → **supporté sur Android Chrome, PAS sur iOS Safari** (Apple refuse, durablement). Donc une **PWA nue ne peut pas parler au T-Beam sur iPhone**. (Contournement debug-only : navigateur **Bluefy** sur iOS.)

**La parade** : ne pas livrer une PWA nue mais **emballer le code web avec [Capacitor](https://capacitorjs.com/)** → vraie app native iOS+Android depuis **une base web** ; BLE via plugin natif `@capacitor-community/bluetooth-le` (CoreBluetooth) → **marche sur iPhone**. Live-reload sur l'iPhone branché.

**Arbitrage Reticulum** : **aucune implé RNS en JS/navigateur** (et le WASM ne sauve pas le BLE iOS). ⇒ on **parle au [[rnode]] directement** (protocole hôte KISS en BLE) + un **mini-protocole tactique en JS**. On **perd** routage multi-saut / chiffrement / [[lxmf]] ; on **gagne** simplicité totale + offline natif + iOS+Android. OK pour proto **mono-escouade vue directe** ; [[reticulum]] réintroduit plus tard **côté device/dorsal** (décision séparable). Voir aussi [[from-scratch-feasibility]].

**Stack** : Capacitor + framework web ; **MapLibre GL JS + PMTiles** (cartes offline mono-fichier) ; assets web embarqués = offline natif (pas de service worker) ; T-Beam en [[rnode]] (KISS) ou mini-firmware custom (porte d'entrée [[gfsk-sub-ghz|GFSK]]). **Recette concrète Svelte → [[svelte-prototype-stack]]**.

## Reco
- **Prototyper vite → Option 1 (fork [[sideband]])** : Reticulum + [[lxmf]] + cartes + BLE/[[rnode]] **déjà câblés** ; une base couvre **Win+Android+macOS** tout de suite ; tu codes l'**UX tactique**, pas la plomberie. iOS = angle mort assumé.
- **Natif + rester Python → Option 2 (BeeWare)** : RNS embarqué, vise iOS aussi, mais **UI à reconstruire** (pas de carte/LXMF offerts) → plus lent.
- **Meilleure UI long terme → Option 3 (Flutter + `rnsd`)** : superbe partout + plugins BLE, mais **2 briques** (Dart ↔ démon) et **iOS = le morceau dur**. Projet d'intégration, pas un proto.
- **Surveiller (2026) → Option 4** : cœur Rust/Go compilé + UI native = seul vrai « single codebase natif » incluant iOS, mais ports immatures → produit, pas proto.

## Le facteur décisif : que signifie « Apple » ?
- **macOS** (desktop de test) → trivial, toutes les options marchent.
- **iPhone dans la boucle de proto** → casse le single-codebase **Python** (Python sur iOS + BLE iOS = mur) → privilégier **Option 5 (Web+Capacitor)** qui contourne le mur via un plugin BLE natif.

**Bottom line :**
- **Debug iPhone-first + Android simple + offline** → **Option 5 (Web + Capacitor, RNode direct)**, Reticulum mis de côté pour le proto (réversible).
- **Si on veut Reticulum embarqué tout de suite** (et iOS non bloquant) → **Option 1 (fork [[sideband]])**, Win+Android+macOS.
- **Produit** → réévaluer Flutter+`rnsd` ou un cœur Rust (Option 3/4).

Liens : [[phone-app-architecture]], [[reticulum]], [[sideband]]
Source : [[2026-06-01_sessions-conception]] · github.com/markqvist/Reticulum, Reticulum-rs (Beechat), go-reticulum, FOSDEM 2026
