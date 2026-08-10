---
type: architecture
tags: [architecture, from-scratch, feasibility, mac, certification]
created: 2026-06-02
updated: 2026-06-02
status: draft
---

# Faisabilité d'une solution « from scratch »

Question : partir de zéro serait-il compliqué ? **Réponse par couche** — la difficulté n'est ni le matériel ni la radio, mais le **protocole** et la **certification**.

| Couche | Difficulté | Verdict |
|---|---|---|
| **Matériel** (PCB maison) | Élevée, inutile | ❌ Le [[lilygo-t-beam]] existe |
| **Driver radio SX1262** | Faible (lib **RadioLib**) | ✅ Trivial |
| **MAC mesh robuste**, centaines de nœuds **mobiles** (sync TDMA, nœud caché, mobilité) | **Très élevée** | ⚠️ LE vrai morceau (mois de travail) |
| **Conformité CE / RED** (EMC + radio) | Élevée, **coûteuse** | ⚠️ Réelle pour un produit commercial |

## Le from-scratch ne supprime pas le 1 %
Le [[duty-cycle-eu868|duty-cycle]] est une **loi** ([[duty-cycle-mitigation]]). Partir de zéro ne la repousse que via **LBT+AFA** et un **MAC efficace** — pas en l'ignorant.

## Le custom le plus léger : fork RNode pour PHY GFSK
Si le but est juste d'utiliser le **[[gfsk-sub-ghz|GFSK]]** (airtime ~10× plus court → moins de duty-cycle/collisions), c'est l'option **la plus petite** : **forker [[rnode]]** pour ajouter un mode PHY GFSK (RadioLib gère la radio), **Reticulum reste inchangé** au-dessus. C'est une **modif firmware**, pas un MAC ni un stack. Coût : framing GFSK, FEC logiciel optionnel, passerelle bi-mode GFSK↔LoRa. Bien plus léger que la ligne « MAC mesh » du tableau.

## Le bon compromis (si MAC nécessaire) : custom SOUS Reticulum
[[reticulum]] est **agnostique au transport** → écrire une **interface RNS custom** qui implémente **ton MAC + LBT/AFA**, tout en **gardant** le chiffrement, le routage et l'écosystème app. On code from-scratch **uniquement la couche airtime** (là où c'est utile), on réutilise le reste. Évite de réinventer crypto + routage.

## Recommandation
1. **Mesurer d'abord** : l'archi segmentée [[reticulum]]/[[rnode]] reste-t-elle sous budget en réel (escouade de 12, position /5-10 s) ? Probablement oui.
2. **Si ça coince** : from-scratch **ciblé** = MAC + LBT/AFA en interface Reticulum (effort borné).
3. **Si seul le GFSK est visé** : fork RNode PHY (option la plus légère, ci-dessus) — mais en **second tour**, après avoir mesuré que le LoRa rapide ne suffit pas.
4. **Full from-scratch** (HW + protocole + app + certif) : réservé à un vrai produit financé.

Liens : [[duty-cycle-mitigation]], [[hierarchical-segmented-network]], [[reticulum]]
Source : [[2026-06-01_sessions-conception]]
