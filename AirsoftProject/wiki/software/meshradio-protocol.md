---
type: software
tags: [software, protocole, meshradio, implémentation, chiffrement]
created: 2026-08-03
updated: 2026-08-11
status: stable
---

# Protocole MeshRadio v2

Protocole binaire **maison** conçu le 03/08/2026, qui remplace (pour le prototype) la couche
[[lxmf]]/[[reticulum]] envisagée en juin. Spécification de référence : `shared/PROTOCOL.md` dans le
dépôt — **v2 depuis D11** (escouades chiffrées). La v1 en clair décrite ici auparavant est
**périmée** : en-tête 17 o, CRC-16, adresses dérivées de la MAC, aucun chiffrement.

## Idée directrice : une spec, deux implémentations verrouillées

Le format est écrit **une seule fois** en markdown, puis implémenté deux fois :

| Implémentation | Fichier | Test |
|---|---|---|
| C++ (firmware) | `firmware/include/protocol.h` | `pio test -e native` |
| TypeScript (app) | `app/src/lib/proto/frames.ts` | `npm test` (91 tests) |

`node shared/gen-vectors.ts` exécute le codec TypeScript et écrit **les octets attendus** dans deux
fichiers : `shared/testvectors.json` (lu par Vitest) et `firmware/include/vectors.h` (lu par Unity
**et** par l'autotest embarqué). Les deux camps sont donc validés contre **la même vérité binaire** :
une divergence d'un seul octet casse un test au lieu de se découvrir sur le terrain.

## Trame

En-tête de **8 octets** (9 en unicast), charge utile ≤ 180, sceau AES-GCM de 8 octets si chiffrée :
`ver_type · ttl_flags · squad · src · seq(u16) · epoch(u16) [· dst]`. Tout en little-endian.
Adresse sur **5 bits** — 31 membres par escouade, attribuée par le chef. Plus de CRC : le sceau GCM
détecte l'altération, et le CRC matériel de LoRa filtre déjà les paquets corrompus.

**Types réseau** : `POSITION` (12 o), `TEXT`, `NODEINFO`, `ACK`, `JOIN_REQUEST`/`JOIN_GRANT` (en
clair, le candidat n'a pas encore la clé), **`MARKER`** (point tactique, cf. ci-dessous).
**Types locaux BLE** (drapeau `LOCAL`, jamais émis en radio) : `CFG_GET`/`CFG_STATE`/`CFG_SET`,
`STATUS` (~1 Hz), `LOG`, `JOIN_EVENT`, `JOIN_CMD`.

## Chiffrement par escouade

**AES-128-GCM**, sceau tronqué à 8 octets, une clé aléatoire de 16 o par escouade distribuée par le
chef via X25519 (D11). Le vecteur d'initialisation n'est **jamais transmis** : il se déduit de
`(src, squad, epoch, seq)`. D'où le champ `epoch`, persisté en NVS — sans lui, `seq` repartant de
zéro à chaque démarrage rejouerait les mêmes vecteurs, ce qui casse la confidentialité.

`ttl_flags` est **exclu** des données authentifiées : un relais le modifie, l'inclure invaliderait le
sceau dès la première retransmission. Défaut invisible à deux nœuds, fatal dès qu'un relais entre en
jeu — figé par un vecteur de test dédié.

## Points tactiques (`MARKER`, D12)

13 octets + libellé : `owner · id · kind · lat · lon · ttlMin · label`. Nomenclature **ATAK** pour
`kind` (ennemi `a-h-G`, véhicule ennemi `a-h-G-E-V`, ami `a-f-G`, incertain `a-u-G`, objectif
`b-m-p-w`, VIP, danger, rassemblement), `kind = 0` valant suppression.

L'identité d'un point est **`(owner, id)`, pas `src`** : le créateur voyage dans la charge utile, ce
qui permet à tout membre de corriger ou retirer le point d'un autre. Chaque point est émis **deux
fois** — une position perdue est remplacée dix secondes plus tard, un point tactique perdu ne revient
jamais. `ttlMin` se compte **à la réception locale**, faute d'horloge commune.

## Comportement mesh

- **Flooding maîtrisé** : `ttl` = 3, déduplication `(squad, src, seq)` sur 64 entrées, ré-émission
  différée. Cf. [[mesh-networking]].
- **Délai de relais pondéré par le SNR** + **annulation du relais redondant** si la trame est
  réentendue pendant l'attente (D10) : sans quoi N nœuds en vue directe émettent N fois chaque trame.
- **Toutes les escouades sont relayées, y compris illisibles.** Le `squad` est un filtre
  d'**appartenance**, pas de propagation : un joueur d'une autre escouade reste un relais utile — il
  retransmet une charge utile qu'il ne peut pas lire (D9).
- **Budget de temps d'antenne** : après une émission de durée `T`, silence pendant
  `T × (100/dutyPercent − 1)`. Implémentation directe de [[duty-cycle-mitigation]].

## Lien BLE

Nordic UART Service + cadrage **SLIP** (RFC 1055, `0xC0`/`0xDB`), identique des deux côtés et testé
sur fragmentation en morceaux de 3 octets — c'est le rôle que joue KISS chez [[rnode]]. Les trames
qui y circulent sont **en clair** : le chiffrement vit entièrement dans le firmware, l'app ne
manipule aucune clé.

## Ce que le protocole ne fait pas

- **Pas d'identité individuelle** : le sceau prouve « un membre de l'escouade », pas « ce
  membre-là ». Le bit `0x20` reste réservé pour une signature.
- **Pas de protection contre le rejeu**, faute de fenêtre de séquence.
- **Pas d'adressage routé** : flooding à TTL. Suffisant à l'échelle d'une escouade, insuffisant pour
  les centaines de joueurs de [[hierarchical-segmented-network]].
- **Pas de reprise d'état** : un arrivant ne reçoit rien de ce qui a été diffusé avant lui.

Liens : [[meshradio-implementation]], [[lxmf]], [[duty-cycle-mitigation]], [[mesh-networking]]
