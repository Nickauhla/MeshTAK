# Firmware MeshRadio — LilyGO T-Beam Supreme

Le T-Beam est un **modem** : il gère la radio mesh LoRa, le GNSS et le lien BLE. Il n'a aucune
interface propre — carte, escouade et messages vivent dans [`../app`](../app).

## Prérequis

[PlatformIO](https://platformio.org/) (extension VS Code, ou `pip install platformio`).

## Compiler, tester, flasher

```bash
pio test -e native
```

Valide que le codec C++ produit **exactement** les octets du codec TypeScript de l'app
(vecteurs générés par `node ../shared/gen-vectors.ts`). À lancer après toute modification du
protocole.

```bash
pio run -e tbeam-supreme -t upload && pio device monitor
```

Au premier démarrage, la console affiche la détection du GNSS puis l'état de la radio :

```
=== MeshRadio — T-Beam Supreme ===
[board] AXP2101 OK — rails radio/GNSS actifs
[main] nodeId=7C3A9F12 indicatif=TB-9F12 escouade=1
[gps] détection du port GNSS…
[gps] détecté : rx=9 tx=8 @38400
[radio] SX1262 OK — 869.525 MHz SF7
[ble] service NUS annoncé sous « MeshRadio-9F12 »
```

## Organisation

| Fichier | Rôle |
|---|---|
| `include/board_pins.h` | Brochage T-Beam Supreme + rails AXP2101 |
| `include/protocol.h` | Codec du protocole (miroir C++ de `app/src/lib/proto/frames.ts`) |
| `src/board.cpp` | PMU AXP2101 : **allume les rails radio (ALDO3) et GNSS (ALDO4)** |
| `src/gps.cpp` | UART GNSS avec détection automatique broches + débit |
| `src/radio.cpp` | SX1262 via RadioLib (TCXO 1,8 V, DIO2 en commutateur d'antenne) |
| `src/mesh.cpp` | Relais, déduplication, file d'émission, budget de temps d'antenne |
| `src/blelink.cpp` | Service NUS NimBLE + cadrage SLIP |
| `src/display.cpp` | Afficheur d'état OLED (U8g2) + extinction automatique |
| `src/settings.cpp` | Configuration persistée en NVS |
| `src/main.cpp` | Assemblage + ordonnancement |

## Afficheur d'état (OLED 1,3")

L'écran **n'est pas une interface** — la carte, l'escouade et les messages restent dans l'app. Il
répond à la seule question qu'on se pose une fois le téléphone rangé : *est-ce que ce boîtier
fonctionne ?*

```
┌────────────────────────┐
│ TB-8EB9          ESC 1 │  ← bandeau inversé : identité + escouade
│ FIX  11 sat  HDOP 1.2  │
│ BATT 87%               │
│ 3 coequipiers   -47 dBm│
└────────────────────────┘
```

Quand le joueur se déclare touché ou éliminé, l'écran bascule en **bandeau plein écran inversé**
(`TOUCHE` / `ELIMINE` / `A L'AIDE` en grands caractères + indicatif) — lisible par un arbitre à
quelques mètres sans manipuler le boîtier.

**Extinction automatique après 30 s** (`SCREEN_TIMEOUT_MS` dans `display.cpp`) : un OLED tire
10-20 mA, ce qui pèse sur une 18650 en partie longue. Réveil sur **appui court du bouton BOOT**, ou
automatiquement dès que le statut du joueur change.

L'écran est **détecté au démarrage** par un scan I2C : sans écran soudé, le firmware fonctionne
normalement et le module devient inopérant. Le scan est affiché sur la console —
`0x3C  <- écran OLED`.

Le contrôleur **SH1106 est confirmé** sur les exemplaires en main (affichage aligné, vérifié le
03/08/2026). Sur un autre exemplaire dont l'image sortirait **décalée de deux pixels**, il s'agirait
d'un SSD1306 : remplacer `U8G2_SH1106_128X64_NONAME_F_HW_I2C` par
`U8G2_SSD1306_128X64_NONAME_F_HW_I2C` dans `display.cpp`. Les deux répondent à la même adresse I2C,
impossible de les distinguer autrement.

## Trace série

Le firmware commente son trafic radio sur la console — indispensable pour diagnostiquer sur le
terrain, là où aucun téléphone n'est branché en USB :

```
[gps] FIX  48.856600, 2.352200  alt=35 m  11 sat  HDOP=1.2
[tx] POS  33 o  62 ms d'antenne
[rx] POS  de 5743E2B2 seq=41 ttl=3  -47 dBm / 9 dB
       48.856612, 2.352190  alt=36 m  10 sat  batt=87%  statut=0
```

Le **RSSI** est la mesure qui compte pour les essais de portée : il chute bien avant que les positions
cessent d'arriver, et donne donc une marge d'alerte.

## Points d'attention matériels

**Les rails d'alimentation.** Sur la Supreme, le SX1262 et le GNSS sont alimentés par le PMU
AXP2101 (ALDO3 et ALDO4). Sans `board::begin()`, la radio ne répond pas sur le SPI et le GNSS reste
muet — c'est la panne n°1 sur cette carte.

**Le port GNSS.** Les sources publiques se contredisent sur l'orientation RX/TX des GPIO 8 et 9.
Plutôt que de parier, le firmware **sonde les deux combinaisons** × trois débits (38400 / 9600 /
115200) jusqu'à obtenir des phrases NMEA au checksum valide, puis mémorise le résultat en NVS. Le
premier démarrage prend donc jusqu'à ~7 s de plus ; les suivants sont immédiats.

**Le TCXO.** Les modules SX1262 LilyGO utilisent un TCXO piloté par DIO3 en 1,8 V et DIO2 comme
commutateur d'antenne. Ces deux réglages sont dans `radio.cpp` : sans eux, la carte semble
fonctionner mais n'émet rien.

**La console série.** La Supreme passe par l'USB natif de l'ESP32-S3 (`ARDUINO_USB_CDC_ON_BOOT=1`).
Si le moniteur reste vide, votre exemplaire a peut-être un pont USB-UART : passez les deux `-D` à `0`
dans `platformio.ini`.

**Toujours brancher une antenne 868 MHz** avant d'alimenter : émettre à vide endommage le PA.

## Bouton BOOT

| Action | Effet |
|---|---|
| Appui court | Diffuse immédiatement position + identité |
| Appui long (> 1,5 s) | Bascule le statut joueur opérationnel ⇄ touché |

## Paramètres radio

Par défaut : **869,525 MHz**, BW 250 kHz, SF7, CR 4/5, +17 dBm, sync word `0x12`, budget de temps
d'antenne 10 %. Modifiables depuis l'app (onglet Réglages → Radio) et persistés en NVS.

Le choix de 869,525 MHz place le canal dans la sous-bande **g3** (869,4–869,65 MHz), qui autorise
**10 %** de rapport cyclique au lieu de 1 % dans la bande 868,0–868,6 MHz — dix fois plus de trafic
pour le même nombre de joueurs. Justification complète dans
[`../shared/PROTOCOL.md`](../shared/PROTOCOL.md) §7.
