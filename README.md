# MeshTAK

Réseau tactique **airsoft** sur LoRa 868 MHz : positions d'escouade et messagerie, **sans réseau
cellulaire**. Deux LilyGO **T-Beam Supreme** (ESP32-S3 + SX1262 + GNSS) servent de modems radio ;
toute l'interface vit dans une application **Android + iOS** connectée en Bluetooth.

> **Nom de code provisoire.** Le nom annoncé en Bluetooth par les boîtiers reste `MeshRadio-XXXX` :
> c'est un identifiant de liaison, partagé par le firmware et l'app, dont le changement impose de
> reflasher toutes les cartes en même temps. À renommer lors du prochain flash coordonné.

```
┌──────────────┐   BLE (SLIP)   ┌─────────────┐   LoRa 869,525 MHz   ┌─────────────┐   BLE   ┌──────────────┐
│  App Svelte  │◀──────────────▶│  T-Beam #1  │◀────────────────────▶│  T-Beam #2  │◀───────▶│  App Svelte  │
│ Android/iOS  │                │ radio + GPS │      mesh, TTL 3     │ radio + GPS │         │ Android/iOS  │
└──────────────┘                └─────────────┘                      └─────────────┘         └──────────────┘
```

## Le principe : un seul protocole, deux implémentations verrouillées

Le format binaire est défini **une fois** dans [`shared/PROTOCOL.md`](shared/PROTOCOL.md), puis
implémenté en C++ (firmware) et en TypeScript (app). Les deux codecs sont validés contre **les mêmes
vecteurs d'octets**, générés depuis l'implémentation TypeScript :

```bash
node shared/gen-vectors.ts
```

produit `shared/testvectors.json` (consommé par les tests de l'app) **et**
`firmware/include/vectors.h`. Si un octet diverge entre le téléphone et le T-Beam, un test casse —
avant la sortie sur le terrain.

La cryptographie suit la même règle, avec une nuance : mbedtls n'existe que sur la carte. Les
vecteurs sont donc aussi vérifiés par un **autotest embarqué** qui tourne au démarrage et confronte
l'AES-GCM et le X25519 de l'ESP32 à ceux d'OpenSSL :

```
[autotest] 65/65 vérifications OK en 611 ms
```

## Arborescence

| Dossier | Contenu |
|---|---|
| [`shared/`](shared/) | Spécification du protocole + générateur de vecteurs de test |
| [`firmware/`](firmware/) | Firmware PlatformIO pour T-Beam Supreme (mesh LoRa, GNSS, BLE) |
| [`app/`](app/) | Application Svelte 5 + Capacitor (une base de code → Android **et** iOS) |
| [`AirsoftProject/`](AirsoftProject/) | Vault Obsidian : décisions de conception, comparatifs, sources |

## Démarrage rapide

```bash
node shared/gen-vectors.ts
```

```bash
cd firmware && pio run -e native -t test && pio run -t upload
```

```bash
cd app && npm install && npm test && npm run dev
```

Détails : [`firmware/README.md`](firmware/README.md) et [`app/README.md`](app/README.md).

## Ce que le système fait aujourd'hui

- **Escouades fermées et chiffrées** — le chef crée l'escouade (`Alpha`, `Bravo`…), son boîtier tire
  une clé au hasard. Chaque joueur qui demande à rejoindre doit être **validé par le chef**, qui lui
  transmet la clé chiffrée pour lui seul (X25519). **Il n'y a pas de mot de passe**, et la clé ne
  traverse jamais le lien Bluetooth : l'app ne la voit pas.
- **Positions** — diffusées toutes les 10 s (réglable), chiffrées en AES-128-GCM. L'app affiche les
  coéquipiers avec distance, batterie, fraîcheur du contact et **cercle d'incertitude GNSS**.
- **Messages** — texte à toute l'escouade ou à un membre précis (avec accusé de réception).
- **Statut joueur** — opérationnel / touché / éliminé / demande d'aide, propagé avec la position et
  affiché en grand sur l'écran du boîtier, lisible par un arbitre.
- **Mesh** — flooding maîtrisé : TTL 3, déduplication, délai de relais **pondéré par le SNR** (le
  nœud le plus éloigné parle en premier) et **annulation du relais redondant**. Budget de temps
  d'antenne conforme à la sous-bande 869,4–869,65 MHz.
- **Les autres escouades relaient sans pouvoir lire.** Un boîtier étranger retransmet une charge
  utile qu'il ne peut pas déchiffrer : la segmentation est une frontière d'appartenance, pas de
  propagation.

## Ce qui n'est pas encore fait

- **Pas d'identité individuelle.** Le sceau prouve « un membre de l'escouade », pas « ce membre-là » :
  tous partagent la même clé. Le bit `0x20` de `flags` est réservé pour une signature, dont le coût
  est chiffré dans `shared/PROTOCOL.md` — non finançable sur chaque position.
- **Pas de dorsal inter-escouades.** L'architecture hiérarchique décrite dans
  [`AirsoftProject/wiki/architecture/hierarchical-segmented-network.md`](AirsoftProject/wiki/architecture/hierarchical-segmented-network.md)
  (chefs-passerelles sur une seconde radio) reste à construire.
- **Cartes hors-ligne à préparer.** Le mode PMTiles est câblé mais l'archive de la zone de jeu est à
  générer et à déposer dans `app/public/maps/`.
- **Aucun essai de portée.** Le système fonctionne de bout en bout sur table ; son comportement en
  forêt, en bâti et à distance réelle reste entièrement à mesurer.

## Conformité

Le firmware applique un budget de temps d'antenne logiciel, mais la conformité EU 868 relève de la
réglementation : vérifiez la sous-bande, le rapport cyclique et la puissance **ERP** (gain d'antenne
compris) avant tout usage sur le terrain.
