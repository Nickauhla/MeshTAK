---
type: software
tags: [software, app, sideband, prototype]
created: 2026-06-01
updated: 2026-06-02
status: stable
---

# Sideband

Application cliente [[reticulum]] / [[lxmf]] (Mark Qvist). **Outil de prototypage zéro-code** du projet : valide toute la chaîne RNode↔hôte↔LoRa sans écrire une ligne.

## Capacités utiles
- **Partage de position / télémétrie P2P** chiffré.
- **Carte hors-ligne** (et en ligne) avec affichage de situation.
- Messagerie chiffrée sur réseaux Reticulum (**LoRa**, packet radio, WiFi, I2P, QR…).
- Connexion à un **[[rnode]] en BLE**.

## Plateformes
Android (APK), Linux, Raspberry Pi, macOS, Windows. **Pas d'iOS** (limite de l'écosystème Reticulum/Kivy, cf. [[phone-app-architecture]]).

## Où télécharger
- **Android** : **F-Droid** (recommandé, store libre), **Google Play**, ou **APK** des *releases* GitHub `markqvist/Sideband`.
- **Desktop** (Linux/macOS/Windows) : `pip install sbapp` puis `sideband`, ou binaires des *releases* GitHub.
- Site officiel : [unsigned.io/sideband](https://unsigned.io/sideband/).

## Mise en route (prototype)
1. Installer Sideband sur un Android. 2. Flasher le [[lilygo-t-beam]] en [[rnode]] (`rnodeconf --autoinstall`). 3. Sideband → **Connectivity → RNode**, appairer en **BLE**. 4. Activer **partage de position** + carte hors-ligne, tester avec un 2ᵉ ensemble.

## Usage projet
Prototype : Sideband sur 2 téléphones + 2 [[lilygo-t-beam]] flashés RNode → test position + messagerie en forêt et intérieur. Plus tard remplacé par une **appli airsoft sur-mesure** (UX dédiée) sur RNS/LXMF, sans toucher au firmware.

⚠️ Licence des apps de l'écosystème souvent **GPLv3** (≠ MIT modifiée du cœur RNS) : OK pour usage/prototype, vigilance si réutilisation de code.

Source : [[2026-06-01_sessions-conception]] · [unsigned.io/sideband](https://unsigned.io/sideband/)
