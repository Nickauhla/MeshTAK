---
type: concept
tags: [concept, radio, lora, sub-ghz]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# LoRa

Modulation radio **longue portée / bas débit** (Semtech), sur bandes **sub-GHz** libres (868 MHz en EU, 915 MHz aux US). Cœur radio du projet : bonne **pénétration** (feuillage, bâti) et portée km, au prix d'un **débit minuscule**.

## Caractéristiques
- Portée : quelques km en ville, 10+ km en vue dégagée.
- Débit : très faible (~0,3–quelques kbit/s selon réglages) → **texte / position / télémétrie**, pas de voix ni streaming.
- **Presets (modem presets)** : compromis portée ↔ débit. Meshtastic « LongFast » (défaut) = portée max, lent. « ShortFast / MediumFast » = plus rapide, moins de portée. **Pour l'airsoft (courtes distances), un preset rapide est préférable.**
- Puce typique du projet : **Semtech SX1262** (présente sur T-Beam, T-Echo, T-Deck, WisBlock…). Sait aussi faire du [[gfsk-sub-ghz|(G)FSK]].

## Pourquoi sub-GHz pour l'airsoft
« Tout terrain, y compris grotte » exige de la pénétration → **868 MHz obligatoire**, le **2,4 GHz est éliminé** ([[radio-technology-alternatives]]).

## Contrainte réglementaire EU
Bande 868 MHz libre mais **duty-cycle ~1 %** → voir [[duty-cycle-eu868]]. C'est cette limite, plus que la portée, qui contraint l'échelle.

Liens : [[mesh-networking]], [[reticulum]], [[meshtastic]]
Source : [[2026-06-01_sessions-conception]]
