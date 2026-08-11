---
type: index
tags: [index]
created: 2026-06-01
updated: 2026-08-03
---

# Index — AirsoftProject

Catalogue de tout le wiki. Point de départ : [[00-overview]] · Thèse : [[synthesis]] · Décisions : [[decision-log]] · Schéma : `CLAUDE.md`.

## Points d'entrée
| Page | Résumé |
|---|---|
| [[00-overview]] | Vue d'ensemble du projet, besoins, contraintes, état des décisions |
| [[synthesis]] | Thèse évolutive (état de la pensée) + questions ouvertes |
| [[decision-log]] | Journal des décisions verrouillées (ADR léger) |

## Concepts (radio / réseau)
| Page | Résumé |
|---|---|
| [[mesh-networking]] | Réseau maillé, flood vs routage adressé, limite grotte |
| [[lora]] | Modulation longue portée sub-GHz, presets, SX1262 |
| [[gfsk-sub-ghz]] | (G)FSK sur SX1262 : débit élevé, courte portée, sweet spot intra-escouade |
| [[duty-cycle-eu868]] | Contrainte ~1 % EU 868, vraie limite d'échelle |
| [[duty-cycle-mitigation]] | Leviers légaux : LBT+AFA, sous-bandes, MAC efficace, calcul d'airtime |

## Logiciels / piles
| Page | Résumé |
|---|---|
| [[reticulum]] | Pile réseau retenue (routage, transport-agnostique, licence) |
| [[meshtastic]] | Firmware mesh de référence, écarté comme base (flood, GPLv3) |
| [[rnode]] | Firmware « modem radio » pour l'hôte (flash, BLE/USB) |
| [[lxmf]] | Protocole de messagerie Reticulum (base des trames tactiques) |
| [[sideband]] | App client (carte, position) — prototype zéro-code |
| [[meshradio-protocol]] | ⭐ Protocole binaire maison **v2** : escouades chiffrées, points tactiques, deux codecs verrouillés par vecteurs partagés |

## Architecture
| Page | Résumé |
|---|---|
| [[hierarchical-segmented-network]] | Escouades segmentées + dorsal via passerelles (clé de l'échelle) |
| [[phone-app-architecture]] | Device headless + UI sur téléphone en BLE (retenue) |
| [[fleet-roles]] | Rôles matériels : joueur / relais / passerelle multi-radio |
| [[from-scratch-feasibility]] | Partir de zéro : difficulté par couche, hybride sous Reticulum |
| [[cross-platform-app-stack]] | Pile appli un seul code : Kivy/Sideband, BeeWare, Flutter+rnsd, Rust, **Web+Capacitor (iPhone-first)** |
| [[svelte-prototype-stack]] | Recette concrète : Svelte 5 + Vite + Capacitor + MapLibre/PMTiles + BLE/KISS |
| [[meshradio-implementation]] | ⭐ **Implémentation réelle** : firmware T-Beam Supreme + app Svelte/Capacitor, pièges matériels |

## Matériel
| Page | Statut | Résumé |
|---|---|---|
| [[lilygo-t-beam]] | ✅ **acheté ×2** | Device joueur — **Supreme ESP32-S3 868**, firmware maison ([[meshradio-implementation]]) |
| [[lilygo-t-echo]] | ❌ abandonné | Acheté en 2 ex. ; e-paper lent, GPS modeste |
| [[lilygo-t-deck-plus]] | ❌ écarté | Tout-en-un ; écran inutile si UI sur téléphone |
| [[rak-wisblock]] | candidat | Relais basse-conso / chemin produit |
| [[heltec-wireless-tracker]] | candidat | Relais / nœud d'appoint headless |

## Comparaisons
| Page | Résumé |
|---|---|
| [[meshtastic-vs-reticulum]] | Routage, modèle, transport, **licences** |
| [[radio-technology-alternatives]] | LoRa / FSK / 2,4 GHz / HaLow — pourquoi sub-GHz |

## Sources
| Page | Résumé |
|---|---|
| [[2026-06-01_sessions-conception]] | Digest des sessions de conception (source brute) |
