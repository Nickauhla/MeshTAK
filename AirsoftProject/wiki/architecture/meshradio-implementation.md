---
type: architecture
tags: [architecture, implémentation, firmware, app, meshradio]
created: 2026-08-03
updated: 2026-08-11
status: draft
---

# Implémentation MeshRadio (firmware + app)

Première implémentation réelle du projet, écrite le 03/08/2026 après réception de **deux
[[lilygo-t-beam|T-Beam Supreme]] ESP32-S3**. Concrétise l'[[phone-app-architecture|architecture
appli-téléphone]] et la [[svelte-prototype-stack|recette Svelte]], avec un écart majeur : le device
tourne sous un **firmware maison** et non sous [[rnode]].

## Répartition

```
App Svelte (Android/iOS) ──BLE/SLIP──▶ T-Beam ──LoRa 869,525 MHz──▶ T-Beam ──BLE──▶ App
   carte, escouade, chat            radio mesh + GNSS + BLE
```

Le T-Beam est un **modem sans interface** : il ne sait que router des trames, lire le GNSS et parler
BLE. Toute l'UX (carte, sélection d'escouade, messagerie, statut joueur) est dans l'app.

## Firmware (`firmware/`, PlatformIO + Arduino)

| Brique | Choix |
|---|---|
| Radio | **RadioLib** sur SX1262 — TCXO 1,8 V via DIO3, DIO2 en commutateur d'antenne |
| GNSS | **TinyGPSPlus** sur UART, avec **auto-détection** broches + débit |
| BLE | **NimBLE-Arduino**, service NUS, tampon SPSC vers la boucle principale |
| PMU | **XPowersLib** / AXP2101 |
| Écran | **U8g2** sur l'OLED 1,3" SH1106 (I2C bus 0, rail ALDO1) |
| Config | NVS (`Preferences`), modifiable depuis l'app |

### L'écran est un afficheur d'état, pas une interface
Ajout du 03/08/2026. La décision D4 (UI sur le téléphone) **reste entière** : l'OLED ne sert pas à
naviguer, il répond à « ce boîtier fonctionne-t-il ? » une fois le téléphone rangé — indicatif,
escouade, fix GNSS et satellites, batterie, nombre de coéquipiers entendus, RSSI. Le statut *touché /
éliminé* bascule en **bandeau plein écran inversé**, lisible par un arbitre à quelques mètres : c'est
le seul usage où l'écran bat le téléphone, parce qu'il s'adresse **aux autres**, pas au porteur.
Contrairement à l'e-paper du [[lilygo-t-echo]] qui avait fait rejeter cette carte, un OLED I2C se
rafraîchit en quelques millisecondes. **Extinction automatique après 30 s** (10-20 mA, non
négligeable sur 18650), réveil au bouton ou sur changement de statut. Détection par **scan I2C** au
démarrage : sans écran soudé, le firmware fonctionne normalement.

Trois pièges matériels documentés, tous capables de faire croire à une carte morte :

1. **Rails d'alimentation.** Sur la Supreme, le SX1262 est sur **ALDO3** et le GNSS sur **ALDO4** du
   PMU AXP2101 (bus I2C1, GPIO 42/41). Sans les allumer, la radio ne répond pas sur le SPI.
   ⚠️ Ce mapping diffère de la T-Beam S3 Core (ALDO2 = radio, ALDO3 = GPS) — ne pas recopier.
2. **Orientation du port GNSS.** La documentation LilyGO et les firmwares tiers se **contredisent**
   sur le sens RX/TX des GPIO 8 et 9. Plutôt que de parier, le firmware **sonde les deux
   combinaisons × trois débits** jusqu'à obtenir des phrases NMEA au checksum valide, puis mémorise
   le résultat en NVS. Coût : ~7 s au premier démarrage seulement.
3. **TCXO et commutateur d'antenne.** Sans `setTCXO(1.8)` + `setDio2AsRfSwitch(true)`, la carte
   s'initialise normalement mais n'émet rien.

## App (`app/`, Svelte 5 + Capacitor 7)

Conforme à [[svelte-prototype-stack]] : Vite + Svelte 5 (runes) + TypeScript, Capacitor 7 pour
iOS/Android, `@capacitor-community/bluetooth-le` (natif CoreBluetooth sur iPhone, Web Bluetooth en
fallback pour itérer dans Chrome desktop), **MapLibre GL JS** + **PMTiles** pour la carte hors-ligne.

Quatre écrans : **Carte** (coéquipiers colorés par statut, points tactiques, suivi, cadrage
escouade), **Escouade** (distance, batterie, fraîcheur, direct/relayé, déclaration de statut, liste
des points tactiques, **quitter l'escouade**), **Messages** (diffusion ou privé avec accusé de
réception), **Réglages** (indicatif, fond de carte, paramètres radio, diagnostic device).

**Écart assumé vs [[svelte-prototype-stack]]** : la source GPS est directement le **GNSS du T-Beam**
(remonté par la trame `STATUS`), pas `@capacitor/geolocation`. Une dépendance en moins, et la
position affichée est exactement celle qui est diffusée en radio.

Les marqueurs de carte sont des **éléments HTML** et non des symboles MapLibre : aucune police de
glyphes à télécharger, donc les indicatifs s'affichent hors ligne.

### Afficher l'incertitude plutôt que la masquer
Constat du 03/08/2026 : un nœud en intérieur a produit un fix sur **3 satellites** (point 2D,
altitude aberrante de 30 m) affiché comme n'importe quelle autre position. Deux options étaient
ouvertes — durcir le seuil à 4 satellites, ou garder le point et **montrer sa qualité**. La seconde
a été retenue : *en forêt, « il est par là à 40 m près » vaut mieux qu'un blanc*, à condition que
l'imprécision soit visible. L'app dessine donc un **cercle d'incertitude en mètres réels** (polygone
GeoJSON, correct à tout zoom), estompe les marqueurs douteux et signale les points 2D.
⚠️ Le rayon est **estimé** (`HDOP × 5 m`, ×3 si 2D), pas mesuré : le récepteur ne transmet aucune
erreur réelle. Ordre de grandeur, pas garantie.

*Ajusté le 11/08/2026* : le cercle **n'est plus dessiné sur un bon fix**. Il y était toujours, donc
il ne signalait rien ; sur beaucoup de satellites, son rayon tient de toute façon dans le marqueur.
Il n'apparaît qu'à partir de « moyen » et grandit ensuite avec le doute — c'est son **apparition**
qui porte l'information, pas sa présence.

### Points tactiques partagés (D12)
Appui long sur la carte (420 ms, tolérance de 12 px, clic droit au bureau) → **menu circulaire**
déployé autour du doigt : ennemi, véhicule ennemi, ami, incertain, objectif, VIP, danger,
rassemblement, selon la nomenclature [[meshradio-protocol|ATAK]]. Le point part chiffré à toute
l'escouade et s'affiche chez tout le monde ; le toucher ouvre une fiche pour le nommer ou le retirer.
La couronne est décalée si l'appui a lieu près d'un bord, mais un repère marque **le point
réellement visé** — le doigt est déjà à l'endroit voulu, le menu ne doit pas le déplacer.

Le menu ne prend ses ordres qu'après 200 ms : il s'ouvre alors que le doigt est **encore posé**, et
sans ce délai le relâchement de l'appui long refermerait le menu qu'il vient d'ouvrir.

### Le lien BLE se rattrape tout seul
Sur le terrain, le téléphone est en poche et le boîtier au harnais : la liaison tombe pour un rien
(portée, veille iOS, batterie). Depuis le 11/08/2026, l'app **rappelle le boîtier** tant que
l'utilisateur n'a pas explicitement coupé — attente croissante 0,7 → 15 s, essai immédiat au retour
au premier plan (iOS suspend l'app, un `setTimeout` en cours peut dater de plusieurs minutes), et
reprise **sans sélecteur** au lancement suivant via `getDevices([id])`, qui fait retrouver le
périphérique connu à CoreBluetooth. L'interface reste en place et affiche le numéro d'essai plutôt
que de renvoyer à l'écran de connexion. « Déconnecter le T-Beam » est la seule commande qui l'oublie.

## Paramètres radio par défaut

**869,525 MHz**, BW 250 kHz, SF7, CR 4/5, +17 dBm, budget 10 %. Le choix de fréquence place le canal
dans la sous-bande **g3** (869,4-869,65 MHz) qui autorise **10 %** de rapport cyclique au lieu du
1 % de la bande 868,0-868,6 — application directe du levier « sous-bandes » de
[[duty-cycle-mitigation]], et **dix fois plus de trafic** à effectif égal.

## État de validation

- ✅ Protocole : **91 tests TypeScript verts**, vecteurs partagés générés ([[meshradio-protocol]]).
- ✅ App : typecheck et build de production propres.
- ✅ **Firmware compilé et flashé sur une vraie carte** (03/08/2026, PlatformIO 6.1.19, USB natif
  ESP32-S3 en `VID_303A/PID_1001`). RAM 12 %, Flash 18,7 % — large marge.
- ✅ **Boot nominal** : AXP2101 détecté et rails actifs, SX1262 initialisé à 869,525 MHz SF7, service
  BLE annoncé. Le brochage et les rails de [[lilygo-t-beam]] sont donc **confirmés en pratique**,
  plus seulement documentés.
- ⚠️ **L'auto-détection GNSS s'est révélée indispensable** : elle a retenu `rx=9 tx=8 @9600`, soit la
  convention des `utilities.h` LilyGO (ESP RX = GPIO 9) et **non** la lecture littérale de la doc
  matérielle. Un pari aurait eu une chance sur deux de rater. *(Claim corrigé : le débit 9600
  n'identifie **pas** un L76K — le MAX-M10S sort aussi à 9600. Les cartes en main sont des M10S,
  réf H659.)*
- ✅ **Chaîne complète validée de bout en bout** (03/08/2026, 2 nœuds `TB-8EB9` et `TB-E2B2`) : les
  deux cartes acquièrent un fix GNSS, la position du nœud distant **arrive par LoRa** et s'affiche
  sur la carte de l'app avec son indicatif et son statut. Cela valide **d'un seul coup** le GNSS, la
  radio 869,525 MHz, le format de trame, le lien BLE, le cadrage SLIP et les deux codecs. Aucune
  configuration n'a été nécessaire : les deux nœuds démarrent sur l'escouade 1.
- ⚠️ **Reste à mesurer sur le terrain** : portée réelle (forêt / bâti), latence de rafraîchissement,
  comportement du relais multi-saut (non exercé avec 2 nœuds en vue directe), tenue du budget de
  temps d'antenne avec une escouade complète.

## Ce qui reste ouvert

- Chiffrement (aucun aujourd'hui).
- Dorsal inter-escouades de [[hierarchical-segmented-network]] (chefs-passerelles, 2ᵉ radio).
- Archive PMTiles de la zone de jeu à générer.
- Retour éventuel vers [[reticulum]] côté device une fois le proto validé — l'arbitrage reste celui
  posé dans [[cross-platform-app-stack]] (option 5).

Liens : [[meshradio-protocol]], [[svelte-prototype-stack]], [[phone-app-architecture]],
[[lilygo-t-beam]], [[duty-cycle-mitigation]], [[from-scratch-feasibility]]
