---
type: software
tags: [software, stack, reticulum, chosen]
created: 2026-06-01
updated: 2026-06-02
status: stable
---

# Reticulum (RNS)

Pile réseau **mesh chiffrée**, open-source, **agnostique au transport** (LoRa, [[gfsk-sub-ghz|FSK]], WiFi, I2P, filaire…). **Base réseau retenue** pour le projet ([[decision-log]]).

## Pourquoi retenu (vs [[meshtastic]])
- **Vrai adressage + vrai routage** (pas du flood) → bien mieux taillé pour l'**échelle** (centaines de nœuds, [[hierarchical-segmented-network]]).
- **Licence MIT modifiée** → plus permissive commercialement que la GPLv3 de Meshtastic. ⚠️ Deux clauses ajoutées : (1) pas d'usage dans un système visant à **nuire intentionnellement à des humains**, (2) pas pour entraîner des datasets IA. La clause (1) est une **zone grise pour l'airsoft** → à lever avec l'auteur ou un juriste. Détail : [[meshtastic-vs-reticulum]].

## Modèle mental (≠ Meshtastic)
RNS **tourne sur un hôte** (téléphone / Pi / PC). La carte LoRa est une simple **interface radio**, flashée avec le firmware [[rnode]], pilotée en **USB ou BLE**. C'est exactement l'[[phone-app-architecture|architecture appli-téléphone]] visée.

## Briques de l'écosystème
- **RNS** (cœur réseau) — `pip install rns` (outils : `rnodeconf`, `rnsd`, `rnstatus`, `rnpath`, `rnprobe`).
- **[[lxmf]]** — protocole de messagerie par-dessus RNS (base des trames tactiques).
- **[[sideband]]** — appli (Android/desktop) : carte hors-ligne + position + messagerie. ⚠️ apps souvent en GPLv3 (≠ licence du cœur).

## Implémentations (état 2026)
Le **Python** reste la seule implémentation **complète/de référence**. En 2026, des ports émergent — **Rust** (Reticulum-rs), **Go** (go-reticulum), **C++** (microReticulum, embarqué), **Zig**, tentative **ReticulumiOS** — mais **jeunes, parité non garantie**. **FOSDEM 2026** : Mark Qvist annonce se retirer → gouvernance communautaire, migration multi-implémentations. ⇒ pour **prototyper**, s'appuyer sur le Python ; surveiller les cœurs natifs (Rust) pour le **produit** (single-codebase natif incluant iOS). Choix de pile applicative : [[cross-platform-app-stack]].

Le démon **`rnsd`** peut **exposer une instance partagée** (clé RPC, y compris Android) → permet à une **UI non-Python** (Flutter, etc.) de parler à Reticulum sans réécrire la pile.

## Démarrage prototypage
Voir [[phone-app-architecture]] et [[decision-log]] : flasher un [[lilygo-t-beam]] en RNode (`rnodeconf --autoinstall`), Sideband sur 2 téléphones en BLE.

Source : [[2026-06-01_sessions-conception]] · [github.com/markqvist/Reticulum](https://github.com/markqvist/Reticulum)
