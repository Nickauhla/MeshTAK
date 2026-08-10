---
type: overview
tags: [overview, airsoft, mesh, project]
created: 2026-06-01
updated: 2026-06-01
status: draft
---

# Vue d'ensemble — Dispositif tactique airsoft

## Le projet
Concevoir un **dispositif tactique d'airsoft** abordable : afficher des **cartes** et sa **position GPS**, et échanger des informations par **radio mesh** sans dépendre du réseau cellulaire (utilisable en zones isolées). C'est l'équivalent civil et low-cost de systèmes tactiques militaires hors de prix.

## Besoins fonctionnels
1. **Positionnement GPS** de chaque joueur.
2. **Émission / réception** radio avec **relais multi-sauts** (atteindre des joueurs hors de portée directe).
3. **Interaction** : ajout d'infos, synchro d'escouade — UI déportée sur **téléphone via Bluetooth**.

## Contraintes structurantes
- **Tout terrain** : forêt, CQB/urbain, **grotte**, mixte → voir [[lora]], [[gfsk-sub-ghz]].
- **Échelle** : centaines de joueurs en **escouades de 6-12** → voir [[hierarchical-segmented-network]].
- **Abordable** et **commercialisable** → voir [[meshtastic-vs-reticulum]] (licences).

## État des décisions (résumé)
- Base réseau : **[[reticulum]]** (retenu).
- UI : **[[phone-app-architecture|approche appli-téléphone]]** (retenue) — device headless.
- Device joueur : **[[lilygo-t-beam]]** flashé en **[[rnode]]** (retenu).
- Prototype : **[[sideband]]** + RNode en BLE (zéro code).
- Détail et historique : [[decision-log]] et [[synthesis]].

## Cartes du wiki
- Concepts radio/réseau : [[mesh-networking]], [[lora]], [[gfsk-sub-ghz]], [[duty-cycle-eu868]]
- Piles logicielles : [[reticulum]], [[rnode]], [[lxmf]], [[meshtastic]], [[sideband]]
- Architecture : [[hierarchical-segmented-network]], [[phone-app-architecture]], [[fleet-roles]]
- Matériel : [[lilygo-t-beam]], [[lilygo-t-deck-plus]], [[lilygo-t-echo]], [[rak-wisblock]], [[heltec-wireless-tracker]]
- Comparaisons : [[meshtastic-vs-reticulum]], [[radio-technology-alternatives]]
- Décisions : [[decision-log]]
