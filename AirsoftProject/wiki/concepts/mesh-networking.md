---
type: concept
tags: [concept, mesh, networking]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# Réseau maillé (mesh)

Réseau décentralisé où **chaque nœud peut relayer** les messages des autres : un paquet saute de nœud en nœud jusqu'à atteindre sa cible, même si l'émetteur et le destinataire ne sont pas à portée directe. C'est le mécanisme central du projet (besoin n°2 : relais multi-sauts). Voir [[00-overview]].

## Deux familles de routage
- **Flood routing (rediffusion)** — chaque nœud rebroadcaste ce qu'il reçoit. Simple, robuste à petite échelle. Utilisé par [[meshtastic]] (« managed flood routing », limité à ~3 sauts par défaut). **Ne passe pas à l'échelle** : à des centaines de nœuds → tempête de rediffusion qui sature le canal.
- **Routage adressé** — vrais identifiants + tables de chemins (proche d'IP). Utilisé par [[reticulum]]. Beaucoup mieux taillé pour de grandes topologies.

## Limite physique (cas grotte)
Le mesh **ne crée pas de signal là où il n'y en a pas** : en grotte, il faut assez de nœuds échelonnés pour que chacun « voie » le suivant. Trou de couverture = besoin d'un **nœud-relais dédié**. Voir [[fleet-roles]].

## Implication projet
Pour des centaines de joueurs, on ne diffuse jamais tout à tous : on **segmente**. Voir [[hierarchical-segmented-network]].

Source : [[2026-06-01_sessions-conception]]
