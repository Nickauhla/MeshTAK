---
type: software
tags: [software, protocole, meshradio, implémentation]
created: 2026-08-03
updated: 2026-08-03
status: stable
---

# Protocole MeshRadio v1

Protocole binaire **maison** conçu et implémenté le 03/08/2026, qui remplace (pour le prototype) la
couche [[lxmf]]/[[reticulum]] envisagée en juin. Spécification de référence : `shared/PROTOCOL.md`
dans le dépôt.

## Idée directrice : une spec, deux implémentations verrouillées

Le format est écrit **une seule fois** en markdown, puis implémenté deux fois :

| Implémentation | Fichier | Test |
|---|---|---|
| C++ (firmware) | `firmware/include/protocol.h` | `pio test -e native` |
| TypeScript (app) | `app/src/lib/proto/frames.ts` | `npm test` (37 tests) |

`node shared/gen-vectors.ts` exécute le codec TypeScript et écrit **les octets attendus** dans deux
fichiers : `shared/testvectors.json` (lu par Vitest) et `firmware/test/native/vectors.h` (lu par
Unity). Les deux camps sont donc validés contre **la même vérité binaire** : une divergence d'un seul
octet casse un test au lieu de se découvrir sur le terrain. C'est la réponse concrète à l'exigence
« même base de code » entre le device et le téléphone.

## Trame

En-tête fixe de **17 octets** + charge utile (≤ 200) + **CRC-16/CCITT-FALSE** :
`magic 0xA7 · version · type · flags · ttl · squad · src(u32) · dst(u32) · seq(u16) · len`.
Tout en little-endian. Identifiant de nœud = 4 derniers octets de la MAC ESP32.

**Types réseau** : `POSITION` (16 o : lat/lon ×1e7, alt, vitesse, cap, sats, HDOP, batterie,
statut joueur), `TEXT` (UTF-8), `NODEINFO` (rôle + indicatif), `ACK`.
**Types locaux BLE** (drapeau `LOCAL`, jamais émis en radio) : `CFG_GET`/`CFG_STATE`/`CFG_SET`,
`STATUS` (télémétrie device ~1 Hz), `LOG`.

## Comportement mesh

- **Flooding maîtrisé** : `ttl` = 3, déduplication `(src, seq)` sur 64 entrées, ré-émission après un
  délai **aléatoire de 200-800 ms** (anti-collision). Cf. [[mesh-networking]].
- **Toutes les escouades sont relayées.** Le `squad` est un filtre d'**affichage**, pas de
  propagation : un joueur d'une autre escouade reste un relais utile. Décision structurante — elle
  découple la segmentation logique (UI) de la topologie radio, et prépare
  [[hierarchical-segmented-network]] sans l'imposer.
- **Budget de temps d'antenne** : après une émission de durée `T`, silence pendant
  `T × (1000/dutyPermille − 1)`. Implémentation directe de [[duty-cycle-mitigation]].

## Lien BLE

Nordic UART Service + cadrage **SLIP** (RFC 1055, `0xC0`/`0xDB`), identique des deux côtés et testé
sur fragmentation en morceaux de 3 octets. Le flux BLE étant fragmenté par le MTU, un cadrage
explicite est indispensable — c'est le rôle que joue KISS chez [[rnode]].

## Ce que le protocole ne fait pas

- **Aucun chiffrement** : les trames circulent en clair, l'escouade n'est pas un secret. Un bit de
  `flags` est réservé. C'est la principale régression assumée vs [[reticulum]].
- **Pas d'adressage routé** : flooding à TTL, pas de tables de routage. Suffisant à l'échelle d'une
  escouade, insuffisant pour les centaines de joueurs de [[hierarchical-segmented-network]].

Liens : [[meshradio-implementation]], [[lxmf]], [[duty-cycle-mitigation]], [[mesh-networking]]
