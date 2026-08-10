# Protocole MeshRadio v2 — source de vérité unique

Ce document définit **le seul format d'échange** du projet. Il est implémenté **deux fois**, à
l'identique, et les deux implémentations sont validées contre les **mêmes vecteurs de test**
(`shared/testvectors.json`) :

| Implémentation | Fichier | Test |
|---|---|---|
| C++ (firmware T-Beam) | `firmware/include/protocol.h` | `pio test -e native` |
| TypeScript (app Android/iOS) | `app/src/lib/proto/frames.ts` | `npm test` |

Toute modification se fait **ici d'abord**, puis dans les deux codecs, puis on régénère les vecteurs
(`node shared/gen-vectors.ts`).

## Ce qui a changé depuis la v1

| | v1 | v2 |
|---|---|---|
| En-tête | 17 o | **8 o** (9 en unicast) |
| Adresse | 4 o (dérivée de la MAC) | **5 bits** — escouade de 31 membres, attribuée par le chef |
| Intégrité | CRC-16 | **sceau AES-GCM** (authentifie *et* détecte) |
| Confidentialité | aucune | **AES-128-GCM par escouade** |
| Appartenance | champ `squad` déclaratif | **clé d'escouade** distribuée par le chef |
| Position (trame complète) | 35 o / 38 ms | **28 o / 33 ms** — chiffrée *et* plus courte |

Le gain de taille finance le chiffrement : la v2 est plus sûre **et** consomme moins de temps
d'antenne que la v1 en clair.

---

## 1. Trame

Tous les entiers sont en **little-endian**.

| Offset | Taille | Champ | Description |
|---|---|---|---|
| 0 | 1 | `ver_type` | version (4 bits de poids fort, = `2`) \| type (4 bits de poids faible) |
| 1 | 1 | `ttl_flags` | ttl (2 bits de poids fort, `0..3`) \| flags (6 bits de poids faible) |
| 2 | 1 | `squad` | identifiant d'escouade — **sélecteur de clé** |
| 3 | 1 | `src` | adresse de l'émetteur dans l'escouade, `1..31` (`0` = non attribuée) |
| 4 | 2 | `seq` | compteur d'émission (u16, boucle) |
| 6 | 2 | `epoch` | compteur de démarrage, persisté en NVS |
| 8 | 1 | `dst` | **présent uniquement** si `F_UNICAST` ; absent = diffusion |
| … | N | `payload` | chiffré si `F_ENCRYPTED` |
| … | 8 | `tag` | sceau AES-GCM tronqué — **présent uniquement** si `F_ENCRYPTED` |

* En-tête = **8 octets** (9 en unicast). Charge utile ≤ **180 octets**.
* La longueur totale vient du paquet LoRa (ou du cadrage SLIP côté BLE) : aucun champ `len`.
* Il n'y a **plus de CRC** : sur la radio, le CRC matériel de LoRa filtre les paquets corrompus, et
  le sceau GCM détecte toute altération de manière bien plus fiable. Une trame **non chiffrée**
  (`JOIN_REQUEST`, `JOIN_GRANT`, trames locales BLE) ne porte donc aucun sceau — elle est protégée
  soit par le CRC LoRa, soit par la fiabilité du lien BLE.

### `epoch` — pourquoi ce champ existe

`seq` repart de zéro à chaque démarrage. Or le vecteur d'initialisation du chiffrement se déduit de
`(src, epoch, seq)` pour ne rien coûter en airtime : **réutiliser un vecteur avec la même clé casse
la confidentialité**. `epoch` est incrémenté et persisté en NVS à chaque démarrage, ce qui garantit
l'unicité pour 65 536 redémarrages.

---

## 2. Types (4 bits)

### Types **réseau** — transmis par radio et par BLE

| Code | Nom | Chiffré | Charge utile |
|---|---|---|---|
| `1` | `POSITION` | oui | §4.1 |
| `2` | `TEXT` | oui | §4.2 |
| `3` | `NODEINFO` | oui | §4.3 |
| `4` | `ACK` | oui | §4.4 |
| `5` | `JOIN_REQUEST` | **non** | §5.2 — le candidat n'a pas encore la clé |
| `6` | `JOIN_GRANT` | **non** | §5.3 — chiffré pour le seul candidat, pas avec la clé d'escouade |

### Types **locaux** — BLE uniquement, jamais émis par radio

Ils portent obligatoirement `F_LOCAL`. Le firmware refuse de les relayer.

| Code | Nom | Sens |
|---|---|---|
| `8` | `CFG_GET` | app → device |
| `9` | `CFG_STATE` | device → app |
| `10` | `CFG_SET` | app → device |
| `11` | `STATUS` | device → app (≈ 1 Hz) |
| `12` | `LOG` | device → app |
| `13` | `JOIN_EVENT` | device → app — §5.4 |
| `14` | `JOIN_CMD` | app → device — §5.5 |

---

## 3. Drapeaux (6 bits de `ttl_flags`)

| Bit | Nom | Signification |
|---|---|---|
| `0x01` | `F_ENCRYPTED` | charge utile chiffrée, sceau présent |
| `0x02` | `F_RELAYED` | positionné par un relais — **muable**, donc hors du sceau |
| `0x04` | `F_UNICAST` | l'octet `dst` est présent |
| `0x08` | `F_WANT_ACK` | l'émetteur attend un `ACK` |
| `0x10` | `F_LOCAL` | trame locale BLE, **interdite** sur la radio |
| `0x20` | — | réservé : **trame signée** (identité individuelle, cf. §7) |

---

## 4. Charges utiles

### 4.1 `POSITION` — 12 octets

| Offset | Taille | Champ | Unité |
|---|---|---|---|
| 0 | 4 | `lat` (i32) | degrés × 1e7 |
| 4 | 4 | `lon` (i32) | degrés × 1e7 |
| 8 | 2 | `alt` (i16) | mètres au-dessus du niveau de la mer |
| 10 | 1 | `sats_hdop` | satellites (4 bits de poids fort, saturé à 15) \| HDOP (4 bits, cf. barème) |
| 11 | 1 | `batt_status` | batterie (4 bits, pas de 6,67 %, `15` = inconnue) \| statut (4 bits) |

Les coordonnées restent en **absolu**, au format v1 inchangé : c'est la donnée la plus critique du
système, la réencoder pour économiser un octet serait un mauvais échange.

Barème HDOP (4 bits) : `0`→ < 0,8 · `1`→ < 1,0 · `2`→ < 1,3 · `3`→ < 1,6 · `4`→ < 2,0 · `5`→ < 2,5 ·
`6`→ < 3,0 · `7`→ < 4,0 · `8`→ < 5,0 · `9`→ < 6,0 · `10`→ < 8,0 · `11`→ < 10 · `12`→ < 15 ·
`13`→ < 20 · `14`→ ≥ 20 · `15`→ inconnu.

Statut joueur : `0` opérationnel · `1` touché · `2` éliminé · `3` demande de l'aide.

> Vitesse et cap ont disparu de la v1 : ils se recalculent de deux positions successives, et le cap
> GNSS n'a de sens qu'en mouvement. Le magnétomètre QMC6310 de la carte fera mieux le jour où on en
> aura besoin.

### 4.2 `TEXT` — 1..180 octets
UTF-8 brut. `dst` porte le destinataire (absent = toute l'escouade).

### 4.3 `NODEINFO` — 1..17 octets
| Offset | Taille | Champ |
|---|---|---|
| 0 | 1 | `role` : 0 joueur · 1 chef · 2 relais · 3 commandement |
| 1 | 0..16 | `callsign` UTF-8 |

### 4.4 `ACK` — 3 octets
| Offset | Taille | Champ |
|---|---|---|
| 0 | 2 | `ackSeq` (u16) |
| 2 | 1 | `ackSrc` (u8) |

### 4.5 `CFG_STATE` / `CFG_SET` — 40 octets
| Offset | Taille | Champ | Notes |
|---|---|---|---|
| 0 | 1 | `squad` | identifiant d'escouade (lecture seule) |
| 1 | 1 | `addr` | adresse dans l'escouade (lecture seule) |
| 2 | 1 | `role` | |
| 3 | 1 | `state` | `0` seul · `1` en attente de validation · `2` membre · `3` chef |
| 4 | 16 | `callsign` | UTF-8 complété de `0x00` |
| 20 | 12 | `squadName` | UTF-8 complété de `0x00` |
| 32 | 4 | `freqHz` (u32) | |
| 36 | 1 | `sf` | |
| 37 | 1 | `txPower` (i8) | |
| 38 | 1 | `posIntervalS` | secondes |
| 39 | 1 | `dutyPercent` | défaut `10` |

La **clé d'escouade ne traverse jamais le lien BLE** : elle naît dans le device (création) ou y
arrive chiffrée par radio (validation). L'app ne la voit pas.

### 4.6 `STATUS` — 20 octets
| Offset | Taille | Champ |
|---|---|---|
| 0 | 1 | `fixValid` |
| 1 | 4 | `lat` (i32) |
| 5 | 4 | `lon` (i32) |
| 9 | 2 | `alt` (i16) |
| 11 | 1 | `sats_hdop` (même codage qu'en §4.1) |
| 12 | 1 | `battPct` |
| 13 | 2 | `battMv` (u16) |
| 15 | 1 | `charging` |
| 16 | 1 | `airtimePercent` |
| 17 | 1 | `lastRssi` — dBm **négatif, stocké en valeur absolue** |
| 18 | 1 | `lastSnr` (i8) |
| 19 | 1 | `peerCount` |

---

## 5. Escouades : création, adhésion, validation

Il n'y a **pas de mot de passe**. Une escouade est définie par une **clé aléatoire de 16 octets**,
créée par son chef et distribuée uniquement aux membres qu'il valide. C'est ce qui permet la
**révocation** : le chef fait tourner la clé et la redistribue à tous sauf à l'exclu.

Chaque device possède une **paire de clés X25519** générée au premier démarrage et conservée en NVS.
Son **empreinte** = 4 premiers octets de `SHA-256(clé publique)`, affichée en hexadécimal pour que le
chef puisse vérifier de visu qui il valide.

### 5.1 Création
Le chef nomme l'escouade (`Alpha`, `Bravo`, `Charlie`, `Delta`, `Echo`, `Foxtrot`, `Golf`, `Hotel`
par défaut). Le device tire une clé aléatoire de 16 octets, se donne l'adresse `1`, le rôle *chef*,
et calcule `squad = SHA-256(nom en majuscules)[0]`. L'identifiant se **déduit du nom** : rien à
coordonner sur le terrain, il suffit d'annoncer « on est sur Alpha ».

> Deux escouades de noms différents peuvent tomber sur le même octet (1 chance sur 256). C'est sans
> conséquence : les clés diffèrent, les trames de l'autre échouent au déchiffrement et sont ignorées.

### 5.2 `JOIN_REQUEST` (en clair, `squad` = escouade visée)
| Offset | Taille | Champ |
|---|---|---|
| 0 | 32 | clé publique X25519 du candidat |
| 32 | 1 | longueur de l'indicatif |
| 33 | n | indicatif UTF-8 |

Émis toutes les 5 s pendant 60 s, `ttl = 1` (pas de relais : la validation est un acte de proximité).

### 5.3 `JOIN_GRANT` (en clair sur la radio, mais **chiffré pour le seul candidat**)
| Offset | Taille | Champ |
|---|---|---|
| 0 | 4 | empreinte du candidat visé |
| 4 | 32 | clé publique X25519 du chef |
| 36 | 12 | vecteur d'initialisation aléatoire |
| 48 | 30 | contenu chiffré (ci-dessous) |
| 78 | 8 | sceau AES-GCM |

Contenu chiffré : `clé d'escouade (16) · adresse attribuée (1) · longueur du nom (1) · nom (12)`.

Clé d'enveloppe = `SHA-256( X25519(privé_chef, public_candidat) || "MESHRADIO-JOIN-v2" )[0..15]`.

Seul le candidat peut la recalculer : un tiers qui capte la trame ne voit que du bruit. Le chef
attribue la **plus petite adresse libre** de `2` à `31`.

### 5.4 `JOIN_EVENT` (local, device → app)
| Offset | Taille | Champ |
|---|---|---|
| 0 | 1 | événement : `0` candidat en attente · `1` validé · `2` refusé · `3` clé reçue · `4` identité locale |
| 1 | 4 | empreinte du candidat |
| 5 | 1 | longueur de l'indicatif |
| 6 | n | indicatif UTF-8 |

L'événement `4` ne concerne aucun candidat : le device y annonce **sa propre** empreinte, en réponse
à `CFG_GET` — donc à chaque connexion de l'app. C'est ce qui permet au candidat de lire son empreinte
sur son téléphone pendant que le chef compare la sienne. La clé privée, elle, ne bouge pas.

### 5.5 `JOIN_CMD` (local, app → device)
| Offset | Taille | Champ |
|---|---|---|
| 0 | 1 | commande : `0` créer · `1` demander à rejoindre · `2` accepter · `3` refuser · `4` quitter |
| 1 | 4 | empreinte visée (commandes `2` / `3`) |
| 5 | n | nom de l'escouade UTF-8 (commandes `0` / `1`) |

---

## 6. Chiffrement

**AES-128-GCM**, sceau tronqué à **8 octets**.

**Vecteur d'initialisation (12 octets)** — jamais transmis, entièrement déduit de l'en-tête :

```
[ src(1) | squad(1) | epoch(2 LE) | seq(2 LE) | 0 0 0 0 0 0 ]
```

**Données authentifiées additionnelles (AAD)** = les champs **immuables** de l'en-tête :

```
[ ver_type(1) | squad(1) | src(1) | seq(2) | epoch(2) | dst(1 si unicast) ]
```

> ⚠️ `ttl_flags` est **exclu du sceau**, et c'est essentiel : un relais décrémente le TTL et
> positionne `F_RELAYED`. Si ces bits entraient dans le calcul, la première retransmission
> invaliderait le sceau et plus personne ne déchiffrerait. Défaut invisible à deux nœuds en vue
> directe, fatal dès qu'un relais entre en jeu.

Une trame dont le sceau ne se vérifie pas est **silencieusement jetée** — mais elle est **relayée
quand même** (§7.3) : ne pas savoir lire n'empêche pas de faire suivre.

---

## 7. Comportement mesh

1. **Déduplication** — clé `(squad, src, seq)` sur 64 entrées. `squad` est indispensable : deux
   escouades ont chacune leur membre n° 3.
2. **Relais** — si `ttl > 1`, `dst != moi` et trame non locale : `ttl -= 1`, `F_RELAYED`, ré-émission
   différée. `ttl` par défaut = `3`.

   Délai **pondéré par le SNR**, de ~150 ms (signal faible) à ~900 ms (signal fort), plus 0-80 ms de
   gigue. Le nœud le plus éloigné parle en premier : c'est son relais qui étend le plus la couverture.
3. **Suppression de relais redondant** — si la même trame `(squad, src, seq)` est réentendue pendant
   l'attente, le relais en file est **annulé**. Sans cette règle, N nœuds en vue directe émettent
   N fois chaque trame (~46 % de temps d'antenne à 6 nœuds).
4. **Toutes les escouades sont relayées, y compris illisibles** — la segmentation est un filtre
   d'appartenance, pas de propagation. Un nœud d'une autre escouade reste un relais utile ; il
   retransmet une charge utile qu'il ne peut pas lire.
5. **Budget de temps d'antenne** — après une émission de durée `T`, silence pendant
   `T × (100 / dutyPercent − 1)`. À 10 % : 9 × `T`.
6. **ACK** — une trame `TEXT` unicast avec `F_WANT_ACK` déclenche un `ACK` immédiat du destinataire.
7. **Trames venues du téléphone** — l'app n'écrit jamais `src`, `seq`, `epoch` ni `ttl` : le firmware
   les renseigne. Une `POSITION` reçue **du téléphone** ne porte pas de coordonnées, elle déclare le
   **statut du joueur** ; le device réémet aussitôt sa vraie position GNSS avec ce statut.

### Ce que le protocole ne protège pas

- **L'analyse de trafic.** `src`, `seq` et les horaires sont en clair : un observateur sait qui émet
  et à quelle cadence, sans savoir où ni quoi.
- **Le vol de boîtier.** La clé d'escouade est en clair dans la NVS ; le chiffrement de flash de
  l'ESP32-S3 n'est pas activé. Qui récupère une carte extrait la clé.
- **L'usurpation entre membres.** Tous les membres partagent la clé : le sceau prouve « un membre de
  l'escouade », pas « ce membre-là ». Le bit `0x20` est réservé pour une signature individuelle, dont
  le coût a été chiffré : ~85 ms au lieu de 33 par position, soit ~25 % d'occupation du canal pour
  une escouade de douze — non finançable sur chaque position, réservé aux trames à enjeu (statut,
  ordre, point tactique).
- **Le rejeu**, faute de fenêtre de séquence.

---

## 8. Lien BLE — cadrage SLIP

**Nordic UART Service** :

| Rôle | UUID |
|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX (app → device) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX (device → app) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

Le flux BLE étant fragmenté par le MTU, chaque trame est encadrée en **SLIP** (RFC 1055) : `0xC0`
délimite, `0xDB` échappe (`0xDB 0xDC` pour un `0xC0` littéral, `0xDB 0xDD` pour un `0xDB`).

Les trames qui circulent en BLE sont **en clair** : le device déchiffre avant de transmettre au
téléphone. Le chiffrement vit **entièrement dans le firmware** — l'app ne manipule aucune clé. C'est
ce qui évite de dépendre de `crypto.subtle`, indisponible hors contexte sécurisé (donc absent en
développement sur `http://<ip-du-lan>:5173`).

La radio, elle, transporte la trame **brute** : LoRa délimite déjà les paquets.

---

## 9. Paramètres radio par défaut (EU 868)

| Paramètre | Valeur | Justification |
|---|---|---|
| Fréquence | **869,525 MHz** | sous-bande **g3** (869,4–869,65 MHz) : **10 %** de duty-cycle contre 1 % en 868,0–868,6 |
| Largeur de bande | **250 kHz** | occupe exactement 869,400–869,650 MHz |
| SF | **7** | portée suffisante en airsoft, temps d'antenne minimal |
| CR | **4/5** | débit maximal |
| Sync word | `0x12` | réseau privé |
| Puissance | **+17 dBm** | marge sous la limite ; le SX1262 monte à +22 dBm |
| CRC matériel | activé | |

> ⚠️ La conformité EU 868 relève de **la réglementation, pas du firmware**. Le budget de temps
> d'antenne est appliqué logiciellement, mais vérifiez la sous-bande et la puissance ERP (gain
> d'antenne inclus) avant tout usage sur le terrain.

### Note sur la quantification de l'airtime

À SF7/CR 4:5, la charge utile est découpée en blocs de **28 bits** : retirer moins de 3,5 octets ne
change **rien** au temps d'antenne. Plancher incompressible ≈ **10 ms** (préambule 6,3 ms + 8
symboles). Inutile de raboter des octets isolés — c'est la **cadence d'émission** qui n'a pas de
plafond, pas la taille des trames.
