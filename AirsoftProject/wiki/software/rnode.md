---
type: software
tags: [software, firmware, rnode, radio-interface]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# RNode (firmware)

Firmware (Mark Qvist) qui transforme une carte **[[lora]]** en **interface radio générique** pilotée par un hôte. C'est ainsi que [[reticulum]] (ou d'autres) utilise une carte comme **modem radio**. **Brique centrale de l'[[phone-app-architecture|architecture appli-téléphone]]** : le device est un RNode, le téléphone fait l'UI.

## Mise en œuvre
- Flash : `rnodeconf --autoinstall` (installeur interactif, détecte la carte, choix fréquence **868 EU**).
- Connexion à l'hôte : **USB** (proto) ou **BLE** (mode téléphone).
- Config RNS dans `~/.reticulum/config` (port, fréquence, bandwidth, spreading factor, coding rate, tx power).

## Variantes
- **markqvist/RNode_Firmware** (officiel) — commencer par là.
- **liberatedsystems/RNode_Firmware_CE** (communautaire) — support matériel plus large.

## Cartes
- ✅ Retenue : [[lilygo-t-beam]] (RNode + bon GPS + batterie).
- Supportée mais abandonnée : [[lilygo-t-echo]] (RNode-compatible, e-ink désactivé pour éviter le burn-in).
- Relais headless : [[heltec-wireless-tracker]], [[rak-wisblock]].

Source : [[2026-06-01_sessions-conception]] · [github.com/markqvist/RNode_Firmware](https://github.com/markqvist/RNode_Firmware)
