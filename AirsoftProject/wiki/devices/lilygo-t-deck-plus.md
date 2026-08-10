---
type: device
tags: [device, lilygo, t-deck-plus, rejected]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# LilyGO T-Deck Plus (écarté)

Terminal tout-en-un envisagé, puis **écarté** ([[decision-log]]) une fois l'[[phone-app-architecture|architecture appli-téléphone]] retenue.

## Specs
- **ESP32-S3** (+ WiFi + **BLE 5 LE**)
- Radio **SX1262** ([[lora]], 433/868/915 — 868 EU dispo, +22 dBm)
- **GPS intégré** + **batterie 2000 mAh**
- Écran couleur **IPS 2,8" 320×240** + **clavier QWERTY** + trackball
- 115×72×20 mm

## Pourquoi écarté
Excellent comme **handheld autonome**, mais si l'UI est sur le **téléphone**, son écran/clavier/trackball sont **payés pour rien**. Le [[lilygo-t-beam]] (modem pur) est plus adapté et moins cher.

## Quand le reconsidérer
Si on revenait à une **UI embarquée** (sans téléphone). À surveiller aussi : variante **T-Deck Pro** (e-paper, meilleure autonomie).

## Réserves notées
Pas durci/étanche (coque nécessaire) ; lisibilité au soleil moyenne ; rendu carto basique sur ESP32-S3 ; autonomie limitée avec GPS+LoRa+écran.

Source : [[2026-06-01_sessions-conception]]
