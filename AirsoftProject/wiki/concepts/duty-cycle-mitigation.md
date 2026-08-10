---
type: concept
tags: [concept, regulation, duty-cycle, lbt, afa, mac]
created: 2026-06-02
updated: 2026-06-02
status: draft
---

# Atténuer le duty-cycle EU 868

Le [[duty-cycle-eu868|duty-cycle ~1 %]] est une **contrainte réglementaire** (ETSI EN 300 220), pas une limite technique : on ne peut pas la « coder » pour la supprimer. Mais la norme prévoit des **leviers légaux**, exploitables surtout par un protocole maîtrisé.

## Levier 1 — LBT + AFA (échappatoire légale)
L'ETSI autorise le **Listen Before Talk + Adaptive Frequency Agility** **à la place** du duty-cycle : un device qui **écoute avant d'émettre** et **saute de fréquence** est **dispensé** de la limite fixe. Ni [[meshtastic]] ni [[rnode]] ne l'implémentent → c'est typiquement ce qu'un **firmware custom** apporterait. Voir [[from-scratch-feasibility]].

## Levier 2 — Plan de sous-bandes
La bande 868 est découpée en sous-bandes au duty-cycle variable (**0,1 % → 10 %**). Répartir intelligemment canaux escouade vs [[hierarchical-segmented-network|dorsal]] sur les bonnes sous-bandes augmente le budget d'airtime global.

## Levier 3 — Airtime plus court via GFSK
Le **[[gfsk-sub-ghz|GFSK]]** transmet ~10× plus vite que le LoRa → **trame ~10× plus courte** (≈5-6 ms vs ≈50 ms) → **~10× plus de trames** sous le même 1 % **et** ~10× moins de collisions. Coût : **portée/robustesse moindres** (pas de gain de traitement) → réserver à l'**intra-escouade**, garder LoRa pour le dorsal. Chemin : **fork [[rnode]] (mode PHY GFSK)**, Reticulum inchangé au-dessus.

## Levier 4 — Efficacité d'airtime (MAC)
[[meshtastic]] **inonde** (flood) → gaspillage d'airtime massif. Un **MAC ordonnancé (TDMA)** ou event-driven utilise une fraction de l'airtime pour la même info. **C'est le vrai gain d'un from-scratch** : pas plus de débit, mais un usage chirurgical du peu d'airtime autorisé.

## Le calcul qui relativise
Trame position ≈ **30-50 ms**. 1 % = **36 s/h** → ~**720 trames/h** = **1 position / 5 s** en restant légal, **par device**. Le per-device n'est pas le goulot ; c'est le **canal partagé** (collisions) → résolu en grande partie par [[hierarchical-segmented-network|la segmentation par escouade]].
→ **L'archi segmentée sur [[reticulum]] couvre probablement déjà le besoin** sans réécriture (à confirmer par mesure terrain).

Source : [[2026-06-01_sessions-conception]] · ETSI EN 300 220
