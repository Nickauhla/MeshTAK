---
type: concept
tags: [concept, radio, fsk, sub-ghz]
created: 2026-06-01
updated: 2026-06-02
status: draft
---

# (G)FSK sub-GHz

La puce du projet (**Semtech SX1262**, présente sur le [[lilygo-t-beam]]) sait aussi faire du **(G)FSK** — une modulation à **débit bien plus élevé** que le LoRa, au prix d'une **portée réduite**, mais **toujours en sub-GHz** (donc bonne pénétration feuillage/bâti).

Débits comparés (SX1262) : **(G)FSK 1,2 → 300 kbps** vs **LoRa 0,018 → 37,5 kbps** (~8× le débit max).

## ⚠️ Capacité puce ≠ fonction utilisable (firmware)
**Correctif important** (02/06/2026) : le SX1262 est *capable* de GFSK, mais le firmware **[[rnode]]** (notre pile [[reticulum]]) opère en **LoRa brut** et **n'expose pas de mode GFSK**. [[meshtastic]] aussi est **LoRa uniquement**.
→ Dans la pile retenue, on est en **LoRa**, pas en GFSK. Exploiter le GFSK = **firmware custom** (sortir du prêt-à-l'emploi).

Donc le « sweet spot intra-escouade » réaliste **aujourd'hui** = **preset LoRa rapide** (ShortFast/Turbo), pas le GFSK. Le GFSK reste une **piste de R&D** (voir ci-dessous).

## GFSK comme levier de duty-cycle (le vrai intérêt)
**Insight (02/06/2026)** : le GFSK n'aide pas que le débit — il **réduit l'airtime par trame**, ce qui attaque directement le [[duty-cycle-eu868|1 %]]. Trame position ≈ **50 ms** en LoRa vs ≈ **5-6 ms** en GFSK (~10×) → **~10× plus de trames** sous le même plafond **et ~10× moins de collisions**. Voir [[duty-cycle-mitigation]].

**Bémol — bilan de liaison** : le LoRa a un **gain de traitement** (~20 dB de sensibilité) que le GFSK n'a pas → **portée plus courte / moins robuste**, surtout forêt/grotte. Donc GFSK **intra-escouade** (courte portée), **LoRa pour le dorsal** (portée + robustesse). Prévoir éventuellement du **FEC logiciel** pour regagner de la robustesse.

## Chemin custom borné (pas un from-scratch complet)
Ce custom-là est **petit** : **forker [[rnode]] pour ajouter un mode PHY GFSK** (RadioLib rend la radio triviale) ; **[[reticulum]] au-dessus ne change pas** (envoie/reçoit des trames sans se soucier de la modulation) → on garde chiffrement, routage, [[lxmf]], [[sideband]]. Pièges : framing GFSK (sync word/CRC), pas de gain de traitement, interop GFSK↔LoRa via passerelle bi-mode. Détail effort : [[from-scratch-feasibility]].

## Arbitrage GFSK vs preset LoRa rapide
Un preset LoRa rapide raccourcit **déjà** l'airtime **en gardant** la robustesse. Le GFSK gagne encore ~8× mais **coûte de la portée**. → **Prototyper d'abord en LoRa rapide + mesurer** ; le fork GFSK est une **optimisation de second tour**, justifiée seulement si l'airtime reste le facteur limitant à grande échelle.

## Positionnement
- Liens **intra-escouade** (courte portée, beaucoup de positions) → **preset LoRa rapide** (dispo maintenant) ou (G)FSK (si firmware custom plus tard).
- **Dorsal** (longue portée, faible débit) → preset LoRa longue portée.
Voir [[hierarchical-segmented-network]] et [[radio-technology-alternatives]].

Source : [[2026-06-01_sessions-conception]]
