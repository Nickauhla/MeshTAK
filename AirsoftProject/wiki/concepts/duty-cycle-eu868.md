---
type: concept
tags: [concept, regulation, eu868, duty-cycle]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# Duty-cycle EU 868 MHz

En Europe, la bande **868 MHz** est libre d'usage mais soumise à un **duty-cycle d'environ 1 %** : un émetteur ne peut transmettre qu'~1 % du temps sur une sous-bande donnée.

## Pourquoi c'est central pour le projet
C'est **la vraie limite** du [[lora]] pour ce projet, davantage que la portée. Avec des centaines de joueurs beaconnant leur position, l'air sature très vite. → On ne peut pas faire un **mesh à plat** de centaines de nœuds ([[mesh-networking]]).

## Conséquences de conception
- **Segmenter** le réseau par escouade pour borner le trafic par canal → [[hierarchical-segmented-network]].
- **Espacer** les envois de position (toutes les 5-15 s plutôt qu'en continu).
- **Réutilisation spatiale** des fréquences pour escouades éloignées.
- À noter : le **2,4 GHz n'a pas cette contrainte** (argument pour, mais pénétration médiocre → écarté) — voir [[radio-technology-alternatives]].

## Atténuation
Leviers légaux (LBT+AFA, plan de sous-bandes 0,1→10 %, MAC efficace) et calcul d'airtime : voir **[[duty-cycle-mitigation]]**. À court terme, **[[hierarchical-segmented-network|la segmentation]] suffit probablement** à rester sous le budget. Le from-scratch n'abolit pas le 1 % : [[from-scratch-feasibility]].

Source : [[2026-06-01_sessions-conception]]
