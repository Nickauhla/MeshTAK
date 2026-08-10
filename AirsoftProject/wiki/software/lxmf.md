---
type: software
tags: [software, protocol, lxmf, messaging]
created: 2026-06-01
updated: 2026-06-01
status: stub
---

# LXMF

**Lightweight Extensible Message Format** — protocole de **messagerie** de l'écosystème [[reticulum]]. Brique standard sur laquelle bâtir l'échange de messages chiffrés, store-and-forward.

## Rôle dans le projet
Support des futures **trames tactiques** de l'appli airsoft sur-mesure : position, **statut joueur** (touché/éliminé), **waypoint**, **ordre**. [[sideband]] est un client LXMF.

## À investiguer
- Format exact de trame tactique (champs, taille — important vu le [[duty-cycle-eu868|duty-cycle]]).
- Diffusion intra-escouade vs filtrage/agrégation vers le dorsal ([[hierarchical-segmented-network]]).

Source : [[2026-06-01_sessions-conception]]
