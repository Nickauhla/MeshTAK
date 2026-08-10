---
type: software
tags: [software, stack, meshtastic, rejected-as-base]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# Meshtastic

Firmware open-source (**GPLv3**) qui transforme des modules **[[lora]]** en réseau **mesh** de messagerie texte + position, sans infrastructure. **Solution de référence** pour ce cas d'usage, et excellent **banc d'essai** — mais **non retenu comme base** finale ([[decision-log]]).

## Ce qu'il offre « gratuitement » (utile à l'airsoft)
- Carte avec positions temps réel (app officielle), **cartes hors-ligne**.
- **Canaux chiffrés AES-256** (un canal = une escouade).
- **Waypoints** = marquage tactique partagé (objectif, ennemi, RDV).
- Messagerie mesh multi-sauts (managed **flood routing**, ~3 sauts par défaut, configurable ≤7).
- App compagnon (Android/iOS) en **BLE**.

## Pourquoi pas retenu comme base
- **Flood routing** → conçu pour ~**dizaines** de nœuds, pas des centaines (tempête de rediffusion). Voir [[mesh-networking]], [[hierarchical-segmented-network]].
- **GPLv3** (copyleft) → contraignant pour un produit propriétaire (obligation d'ouvrir les modifs distribuées). [[reticulum]] (MIT modifiée) est plus souple. Voir [[meshtastic-vs-reticulum]].

## Rôle résiduel
Banc d'essai rapide / comparaison de latence-portée ; les radios compatibles restent réutilisables.

Source : [[2026-06-01_sessions-conception]]
