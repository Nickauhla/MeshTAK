---
type: overview
tags: [synthesis, thesis]
created: 2026-06-01
updated: 2026-06-02
status: draft
---

# Synthèse — thèse évolutive

> Page vivante : l'état actuel de la pensée du projet. Mise à jour à chaque nouvelle source/décision.

## Thèse actuelle (01/06/2026)
Un système tactique airsoft abordable et passant à l'échelle se construit sur **trois piliers** :

1. **Radio sub-GHz 868 MHz.** Seule techno couvrant *tous* les terrains (forêt + grotte exigent de la pénétration → [[lora]] / [[gfsk-sub-ghz]]). Le 2,4 GHz est éliminé. Limite physique assumée : en grotte, seul le **relais de proche en proche** marche, avec parfois des nœuds-relais dédiés. Voir [[radio-technology-alternatives]].

2. **Architecture réseau hiérarchique, pas un PHY magique.** Le vrai goulot à l'échelle n'est **pas le débit** (les trames sont minuscules) mais le **[[duty-cycle-eu868|duty-cycle]]** réglementaire. Un mesh à plat de centaines de nœuds s'effondre. La solution est de **segmenter par escouade** (canal local dense) + **dorsal** relié par les chefs-passerelles. Voir [[hierarchical-segmented-network]]. Leviers d'atténuation (LBT+AFA, sous-bandes, MAC efficace) : [[duty-cycle-mitigation]] — mais la segmentation suffit probablement déjà.

3. **Séparation device/UI.** Le device est un **modem radio+GPS headless** ([[rnode]]) ; le **téléphone** porte la carte et l'UI en BLE ([[phone-app-architecture]]). C'est l'architecture native de [[reticulum]].

## Choix de pile
**[[reticulum]]** plutôt que **[[meshtastic]]** : routage/adressage réels (mieux taillés pour l'échelle) et licence plus permissive commercialement. Meshtastic reste un excellent **banc d'essai** ; la couche de scaling hiérarchique est à construire ([[meshtastic-vs-reticulum]]).

## Choix matériel
Device joueur = **[[lilygo-t-beam]]** 868 en RNode (bon GPS, grosse batterie, BLE, pas cher). **[[lilygo-t-echo]]** abandonné (e-paper lent, GPS modeste). **[[lilygo-t-deck-plus]]** écarté (écran/clavier inutiles si UI sur téléphone). Flotte hétérogène : voir [[fleet-roles]].

## Questions ouvertes / à investiguer
- Plan de fréquences EU 868 (sous-canaux, réutilisation spatiale) pour N escouades.
- Format de **trame tactique** (position, statut « touché/éliminé », waypoint, ordre) sur [[lxmf]].
- Passerelle multi-radio : [[fleet-roles]] (1 puce LoRa = 1 fréquence à la fois → 2 radios).
- Autonomie réelle terrain ; durcissement/étanchéité du device et du téléphone.
- Lever la clause « no harm » de la licence Reticulum pour usage airsoft commercial.
- **Mesurer** si l'archi segmentée reste sous le budget duty-cycle en réel (escouade de 12, position /5-10 s) → décide si un MAC custom est nécessaire. Voir [[from-scratch-feasibility]].
- Si from-scratch un jour : le faire en **interface custom SOUS Reticulum** (MAC + LBT/AFA), pas un système entier ([[from-scratch-feasibility]]).
