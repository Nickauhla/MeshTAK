---
type: architecture
tags: [architecture, scaling, hierarchy, key-decision]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# Réseau hiérarchique segmenté

**Conclusion architecturale clé du projet.** Un mesh à plat de centaines de nœuds est impossible en [[lora]] (tempête de rediffusion + [[duty-cycle-eu868|duty-cycle 868]]). La solution **mime les réseaux tactiques militaires** : pas un réseau, mais une **hiérarchie de petits réseaux**.

## Principe
```
ESCOUADE A (6-12)     ESCOUADE B (6-12)     ESCOUADE C ...
canal/fréq. A         canal/fréq. B         canal/fréq. C
preset rapide         preset rapide         ...
toutes positions      toutes positions
partagées localement  partagées localement
     │                     │                     │
 [Chef A] ──────────── [Chef B] ──────────── [Chef C]
     └──────────── RÉSEAU DORSAL (backbone) ───────┘
            canal commun, preset longue portée
   transporte SEULEMENT : position des chefs +
   waypoints tactiques + messages de commandement
```

## Règles
- **1 escouade = 1 canal/fréquence** : le trafic dense (6-12 nœuds, positions fréquentes) reste **local** → aucun canal ne porte jamais des centaines de nœuds.
- **Chefs d'escouade = passerelles** : un pied dans le canal escouade, un pied dans le **dorsal**. Ne font remonter que l'**essentiel filtré/agrégé**.
- **Réutilisation spatiale des fréquences** pour escouades éloignées.
- Résultat : ~30 chefs pour 300 joueurs sur le dorsal → soutenable même en LoRa longue portée.

## Implications
- Dépasse [[meshtastic]] brut → couche applicative custom ou [[reticulum]] (retenu).
- Les passerelles ont besoin de **2 radios** (1 puce = 1 fréquence) → [[fleet-roles]].
- C'est **la segmentation** (pas un meilleur PHY) qui rend l'échelle possible.

Liens : [[mesh-networking]], [[phone-app-architecture]], [[synthesis]]
Source : [[2026-06-01_sessions-conception]]
