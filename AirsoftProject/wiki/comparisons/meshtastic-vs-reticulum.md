---
type: comparison
tags: [comparison, meshtastic, reticulum, license]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# Comparaison — Meshtastic vs Reticulum

Arbitrage de la **base réseau**. **Conclusion : [[reticulum]] retenu** ([[decision-log]]).

| Critère | [[meshtastic]] | [[reticulum]] (RNS) |
|---|---|---|
| Routage | Managed **flood** (~dizaines de nœuds) | **Adressage + routage** réels (échelle) |
| Modèle | App embarquée sur le device | RNS sur **hôte**, device = [[rnode|RNode]] radio |
| Transport | LoRa | **Agnostique** (LoRa, FSK, WiFi, I2P, filaire) |
| Licence cœur | **GPLv3** (copyleft) | **MIT modifiée** (plus permissive) |
| UI prête | App officielle (carte, waypoints) | [[sideband]] (carte, position, messagerie) |
| Échelle (centaines) | Mal (tempête de rediffusion) | Mieux taillé |

## Licences (point commercial)
- **Meshtastic GPLv3** : si tu distribues des modifs du firmware, tu dois ouvrir ton code. Contraignant pour du propriétaire.
- **Reticulum MIT modifiée** : permet l'usage commercial (use/sell), **mais** deux clauses ajoutées :
  1. ❗ Pas d'usage dans un système visant à **nuire intentionnellement à des humains** → **zone grise pour l'airsoft** (à lever avec l'auteur ou un juriste : un outil de comm/carto ne « nuit » pas, mais le contexte est une simulation de combat).
  2. Pas pour entraîner des datasets IA.

→ Paradoxe : **Reticulum est plus souple commercialement que Meshtastic** sur le cœur réseau.

## Décision
Reticulum comme base ; Meshtastic conservé comme **banc d'essai**. Voir [[synthesis]].

Source : [[2026-06-01_sessions-conception]]
