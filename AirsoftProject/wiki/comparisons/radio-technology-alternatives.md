---
type: comparison
tags: [comparison, radio, lora, fsk, 2.4ghz, halow]
created: 2026-06-01
updated: 2026-06-02
status: stable
---

# Comparaison — Technologies radio

Quatre curseurs, on ne peut pas tout maximiser : **portée ↔ débit ↔ pénétration ↔ conso/coût**. Pour l'airsoft, la limite du [[lora]] n'est pas la portée mais le **débit + [[duty-cycle-eu868|duty-cycle]]**.

| Option | Débit | Portée (terrain) | Pénétration | Verdict airsoft |
|---|---|---|---|---|
| **LoRa preset rapide** (ShortFast/Turbo) | faible++ | bonne | excellente (sub-GHz) | Réglage gratuit à faire d'abord |
| **[[gfsk-sub-ghz|(G)FSK sur SX1262]]** | élevé (jusqu'~300 kbps) | moyenne | bonne (sub-GHz) | Puce capable, **mais firmware custom requis** (⚠️ voir note) |
| **2,4 GHz** (ESP-NOW/nRF24) | élevé, pas de duty-cycle | courte (200-400 m) | **mauvaise** (feuillage/murs) | ❌ Éliminé (terrain « tout terrain + grotte ») |
| **WiFi HaLow** (802.11ah, 900 MHz) | très élevé | bonne (~1 km) | bonne | Puissant mais cher/jeune, gourmand |
| **DMR / radio amateur** | — | très bonne | bonne | Hors-sujet (licence) |

> ⚠️ **Note GFSK (02/06/2026)** : le SX1262 ([[lilygo-t-beam]]) *gère* le GFSK au niveau puce, mais ni [[rnode]] (Reticulum) ni [[meshtastic]] ne l'exposent — ils opèrent en **LoRa brut**. L'exploiter exige un **firmware custom**. Levier réaliste à court terme = **preset LoRa rapide**. Détail : [[gfsk-sub-ghz]].

## Conclusion
« Tout terrain, y compris grotte » → **pénétration prioritaire** → **sub-GHz 868 MHz obligatoire**, **2,4 GHz éliminé**. Sweet spot intra-escouade réaliste = **preset LoRa rapide** (GFSK = piste R&D firmware) ; dorsal = **preset LoRa longue portée**.

Note : le vrai levier d'échelle n'est pas le PHY mais le **[[hierarchical-segmented-network|découpage réseau]]** + la **fréquence d'envoi de position**.

Source : [[2026-06-01_sessions-conception]]
