---
type: device
tags: [device, lilygo, t-echo, abandoned]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# LilyGO T-Echo (abandonné)

Premier matériel acheté (**2 unités**) pour tests. **Abandonné** ([[decision-log]]).

## Specs
- MCU **nRF52840** (+ BLE)
- Radio **SX1262** ([[lora]])
- **GPS** intégré (modeste, fix lent)
- **Écran e-paper** (encre électronique)

## Pourquoi « très très lent » / abandonné
- **E-paper** : rafraîchissement 1-2 s, clignote → catastrophique pour une carte interactive. (Ce n'est pas [[meshtastic]] qui est lent, c'est l'écran.)
- **GPS modeste**, premier fix souvent long.
- Dans l'[[phone-app-architecture|archi appli-téléphone]], l'écran ne sert à rien.

## Note
RNode-compatible (e-ink désactivé pour éviter le burn-in) → utilisable en banc d'essai/portée, mais sous-optimal. Remplacé par le [[lilygo-t-beam]].

Source : [[2026-06-01_sessions-conception]]
