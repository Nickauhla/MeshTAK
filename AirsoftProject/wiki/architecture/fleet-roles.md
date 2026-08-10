---
type: architecture
tags: [architecture, hardware, roles]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# Rôles de flotte (matériel hétérogène)

L'[[hierarchical-segmented-network|architecture hiérarchique]] implique **plusieurs rôles**, chacun avec un device optimal différent. Penser **flotte**, pas device unique.

| Rôle | Device conseillé | Raison |
|---|---|---|
| **Joueur / chef d'escouade** | [[lilygo-t-beam]] (RNode) + téléphone (BLE) | Modem GPS robuste + UI sur tél |
| **Nœud-relais** (déposé en forêt/grotte) | [[heltec-wireless-tracker]] / [[rak-wisblock]] headless | Pas d'écran, batterie, basse conso, pas cher |
| **Passerelle / commandement** | Raspberry Pi + **2 RNodes** USB | Besoin de 2 radios + CPU pour agréger |

## Point clé : passerelles multi-radio
Une puce **[[lora|LoRa]] ne fait qu'une fréquence à la fois**. Un chef qui doit être sur **son canal escouade ET le dorsal** a besoin de **deux radios**. En prototypage : **Pi + 2 RNodes USB**. À intégrer dès la conception.

## Relais en grotte
Le mesh ne crée pas de signal : prévoir des **nœuds-relais dédiés** dans les zones sans joueur (boyaux longs). Voir [[mesh-networking]].

Liens : [[phone-app-architecture]], [[hierarchical-segmented-network]]
Source : [[2026-06-01_sessions-conception]]
