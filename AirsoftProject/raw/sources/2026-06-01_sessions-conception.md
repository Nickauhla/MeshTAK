---
type: source
tags: [source, conversation, design]
created: 2026-06-01
updated: 2026-06-01
status: stable
---

# Source — Sessions de conception (01/06/2026)

Source brute **immuable** : digest des échanges de conception entre l'utilisateur (Nicolas) et l'agent LLM. C'est la matière première dont tout le wiki est dérivé.

## Contexte exprimé par l'utilisateur
- Objectif : un device pour l'**airsoft** affichant des **cartes** + sa **position GPS**, et transmettant des infos par **radio** (indépendant du réseau cellulaire, utilisable en zones isolées).
- Inspiration : systèmes tactiques **militaires**, mais trop chers pour le civil → viser l'**abordable**.
- Trois parties : (1) position GPS, (2) émission/réception avec **relais multi-sauts**, (3) interaction (potentiellement via **téléphone en Bluetooth**).
- A acheté **2 LilyGO T-Echo** pour tests → trouve l'usage **« très très lent »**.

## Exigences clarifiées au fil des échanges
- **Terrain** : tout type — forêt dense, CQB/urbain, **grotte**, mixte. Doit marcher **partout**.
- **Échelle** : grosses opérations de **plusieurs centaines de joueurs**, découpées en **escouades de 6-12**.
- **Usage** : commercial envisagé (d'où la question des licences).

## Points clés établis (résumé des réponses du LLM)
1. Lenteur du T-Echo = **e-paper** (rafraîchissement 1-2 s) + preset radio **LongFast** par défaut. Pas un défaut de Meshtastic en soi.
2. **Sub-GHz 868 MHz obligatoire** (pénétration forêt/grotte) → 2,4 GHz éliminé comme radio principale.
3. Mesh à plat de centaines de nœuds **impossible** (tempête de rediffusion + duty-cycle 868 ~1 %). → **architecture hiérarchique segmentée** par escouade + dorsal via passerelles.
4. **Reticulum** retenu plutôt que Meshtastic (routage/adressage pour l'échelle ; licence plus permissive commercialement).
5. **Architecture appli-téléphone** retenue : device = RNode (radio+GPS), téléphone = carte+UI en BLE.
6. **Device retenu** : LilyGO **T-Beam 868** en RNode (T-Echo abandonné ; T-Deck Plus écarté car écran inutile si UI sur téléphone).
7. Prototype zéro-code : **Sideband** + RNode en BLE.

## Décisions verrouillées
- Base logicielle : **Reticulum**.
- UI : **appli téléphone** (device headless).
- Device joueur : **T-Beam 868** flashé RNode.
- Prochaine étape : commander 2-3 T-Beam, flasher en RNode, Sideband sur 2 téléphones, refaire un test terrain (forêt + intérieur).
