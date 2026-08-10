---
type: decision
tags: [decision, adr, log]
created: 2026-06-01
updated: 2026-08-03
status: stable
---

# Journal des décisions (ADR léger)

Décisions de conception verrouillées, avec justification. Statuts : ✅ retenu · ❌ écarté · 🕒 ouvert.

## ✅ D1 — Radio sub-GHz 868 MHz (2,4 GHz écarté)
**Pourquoi** : terrain « tout type y compris grotte » → pénétration prioritaire. Le 2,4 GHz meurt en feuillage/roche.
**Réf** : [[radio-technology-alternatives]], [[lora]].

## ✅ D2 — Architecture réseau hiérarchique segmentée
**Pourquoi** : mesh à plat de centaines de nœuds impossible (rediffusion + [[duty-cycle-eu868|duty-cycle]]). Segmenter par escouade + dorsal via chefs-passerelles.
**Réf** : [[hierarchical-segmented-network]].

## ✅ D3 — Base réseau : Reticulum (Meshtastic écarté comme base)
**Pourquoi** : routage/adressage réels (échelle) + licence MIT modifiée plus permissive commercialement que GPLv3.
**Réf** : [[meshtastic-vs-reticulum]], [[reticulum]].

## ✅ D4 — Architecture appli-téléphone (UI déportée)
**Pourquoi** : device = modem headless [[rnode]], téléphone = carte+UI en BLE. Architecture native de Reticulum ; évite l'e-paper lent.
**Réf** : [[phone-app-architecture]].

## ✅ D5 — Device joueur : LilyGO T-Beam 868 (T-Echo abandonné, T-Deck Plus écarté)
**Pourquoi** : modem pur idéal (radio+GPS+batterie+BLE, pas cher). T-Echo lent (e-paper, GPS modeste) ; T-Deck Plus = écran inutile si UI sur téléphone.
**Réf** : [[lilygo-t-beam]], [[lilygo-t-echo]], [[lilygo-t-deck-plus]].

## ✅ D6 — Prototype zéro-code : Sideband + RNode (BLE)
**Pourquoi** : valider la chaîne radio sans coder, comparer latence/portée à Meshtastic.
**Réf** : [[sideband]].

## ✅ D7 — Firmware maison sur le T-Beam (RNode/Reticulum reporté) — *2026-08-03*
**Pourquoi** : l'utilisateur demande explicitement un micrologiciel gérant lui-même mesh + GPS, et une
app iOS+Android d'une seule base de code. Or il n'existe pas d'implémentation [[reticulum]] en
JavaScript, et [[sideband]] ne couvre pas iOS — c'est l'arbitrage déjà posé en option 5 de
[[cross-platform-app-stack]]. On code donc un protocole binaire minimal des deux côtés.
**Ce qu'on perd** : chiffrement, routage adressé, écosystème [[lxmf]]. **Ce qu'on gagne** : iOS +
Android d'un seul code, contrôle total de la trame, offline natif. **Réversible** : Reticulum peut
revenir côté device/dorsal plus tard.
**Réf** : [[meshradio-protocol]], [[meshradio-implementation]], [[cross-platform-app-stack]].

## ✅ D8 — Canal par défaut 869,525 MHz (sous-bande g3, 10 %) — *2026-08-03*
**Pourquoi** : la sous-bande **g3** (869,4-869,65 MHz) autorise **10 %** de rapport cyclique contre
1 % en 868,0-868,6. En BW 250 kHz, le canal occupe exactement la sous-bande. **Dix fois plus de
trafic** à effectif égal, sans rien changer au matériel — le levier le plus rentable de
[[duty-cycle-mitigation]].
**Réf** : [[duty-cycle-eu868]], [[meshradio-implementation]].

## ✅ D9 — Le `squad` filtre l'affichage, pas la propagation — *2026-08-03*
**Pourquoi** : un nœud d'une autre escouade reste un **relais utile**. Le firmware relaie donc toutes
les trames et remonte tout au téléphone ; l'app filtre sur l'escouade. Découple la segmentation
logique de la topologie radio et prépare [[hierarchical-segmented-network]] sans l'imposer.
**Réf** : [[meshradio-protocol]].

## ✅ D10 — Suppression de relais redondant + délai pondéré par le SNR — *2026-08-03*
**Défaut identifié en trace** : le firmware relayait **sans condition** toute trame à `ttl > 1`, même
quand tous les nœuds s'entendaient directement (chaque `[rx] POS` déclenchait un `[tx] POS`). À N
nœuds en vue directe, une position coûtait **N émissions** → ~46 % de temps d'antenne à 6 nœuds,
saturation du budget de 10 % dès 4-5 joueurs, débordement de la file, pertes de positions.
**Invisible à 2 cartes, bloquant à l'escouade complète** — le pire profil de défaut.
**Correctif, deux règles couplées** : (1) le délai avant relais est **pondéré par le SNR**
(~150 ms si signal faible → ~900 ms si signal fort) : le nœud le plus éloigné, dont le relais étend le
plus la couverture, parle en premier ; (2) tout relais en file est **annulé** si la même trame
`(src, seq)` est réentendue pendant l'attente. N retombe à ~2-3 quelle que soit la taille de
l'escouade, sans perte de couverture.
**⚠️ Non vérifié en pratique** : la règle d'annulation exige **3 nœuds** pour se déclencher ; à deux
cartes personne d'autre ne relaie, donc rien à annuler. Compilé, flashé, logique conforme à la spec —
comportement réel à confirmer.
**Réf** : [[meshradio-protocol]] §6, [[duty-cycle-mitigation]], [[hierarchical-segmented-network]].

## ✅ D11 — Protocole v2 : escouades chiffrées, validées par un chef — *2026-08-03*
**Pourquoi** : le champ `squad` de la v1 n'était qu'un filtre d'affichage — n'importe quel boîtier
lisait tout. L'utilisateur voulait un groupe fermé, avec **création d'escouade** et **validation par
un admin**.
**Ce qui a été retenu** :
- **Aucun mot de passe.** Une escouade = une **clé aléatoire de 16 octets** créée par le chef,
  transmise chiffrée pour le seul candidat validé via **X25519** (curve25519 confirmé présent dans le
  framework ESP32). Permet la **révocation**, qu'un mot de passe partagé ne permet pas.
- **Le chiffrement vit entièrement dans le firmware** (AES-128-GCM, sceau tronqué à 8 o). L'app ne
  manipule aucune clé — ce qui évite `crypto.subtle`, indisponible hors contexte sécurisé, donc
  absent en développement sur `http://<ip-lan>:5173`.
- **En-tête dégraissé** : 17 → 8 octets, adresse sur **5 bits** (31 membres, attribuée par le chef),
  plus de CRC (le sceau GCM le remplace). ⇒ position **28 o / 33 ms chiffrée**, contre 35 o / 38 ms
  en clair en v1 : le dégraissage finance le chiffrement.
- **`ttl` et `flags` exclus des données authentifiées** — sans quoi le premier relais invaliderait le
  sceau. Défaut invisible à 2 nœuds, fatal en escouade.
- **Compteur de démarrage (`epoch`)** dans l'en-tête : `seq` repartant de zéro à chaque boot, il
  garantit l'unicité des vecteurs d'initialisation.
**Vérification** : 76 tests côté app + **autotest embarqué 65/65** confrontant mbedtls à OpenSSL sur
la carte (seul endroit possible, mbedtls n'existant pas sur PC).
**Réf** : [[meshradio-protocol]], [[meshradio-implementation]].

## 🕒 Décisions ouvertes
- **Identité individuelle** : le sceau prouve « un membre de l'escouade », pas « ce membre-là ». Bit
  `0x20` réservé pour une signature. Coût chiffré : ~85 ms au lieu de 33 par position, soit ~25 %
  d'occupation du canal à douze joueurs ⇒ à réserver aux trames à enjeu (statut, ordre, waypoint),
  jamais sur chaque position.
- **Vol de boîtier** : la clé d'escouade est en clair en NVS (chiffrement de flash ESP32-S3 non
  activé). Qui récupère une carte extrait la clé.
- Plan de fréquences EU 868 (sous-canaux, réutilisation spatiale) au-delà du canal unique de D8.
- Implémentation passerelle multi-radio ([[fleet-roles]]) et dorsal inter-escouades.
- Lever la clause « no harm » de la licence Reticulum — **suspendue** tant que D7 tient.
- Retour éventuel à [[reticulum]] côté device après validation terrain.

## Prochaine étape
Flasher les **deux [[lilygo-t-beam]] Supreme** reçus (`pio run -t upload`), vérifier la détection GNSS
et l'appairage BLE, puis **premier test terrain à deux** (forêt + intérieur) : portée, latence,
fraîcheur des positions. C'est la première confrontation du code au matériel — rien n'est validé
avant.
