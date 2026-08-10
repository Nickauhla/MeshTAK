---
type: device
tags: [device, lilygo, t-beam, chosen]
created: 2026-06-01
updated: 2026-08-03
status: stable
---

# LilyGO T-Beam (retenu — device joueur)

**Device joueur retenu** ([[decision-log]]), dans l'[[phone-app-architecture|architecture appli-téléphone]].

## 🛒 Acheté : 2 × T-Beam Supreme ESP32-S3 avec GPS *(2026-08-03)*
Deux exemplaires en main. **Changement de logiciel embarqué** : ils ne sont pas flashés en [[rnode]]
mais avec un **firmware maison** (mesh LoRa + GNSS + BLE) — cf. décision D7 du [[decision-log]] et
[[meshradio-implementation]].

**Brochage confirmé en pratique** (boot nominal le 03/08/2026) : SX1262 sur SCK 12 / MISO 13 /
MOSI 11 / CS 10 / DIO1 1 / BUSY 4 / RST 5 · I2C0 17/18 (OLED, capteurs) · I2C1 42/41 (**AXP2101** +
RTC) · bouton BOOT 0.

**Exemplaires en main** : réf **H659** — *T-Beam Supreme V3.0 + T-Beam Core V3.0*, **u-blox MAX-M10S**,
868 MHz **SX1262**, achetés chez **Sino Sear**. C'est la variante idéale identifiée en juin (M10S
plutôt que L76K).

**GNSS : `rx=9 tx=8 @9600` (mesuré).** L'orientation RX/TX des GPIO 8-9 est **contradictoire entre
les sources** ; l'auto-détection du firmware a tranché en faveur de la convention des `utilities.h`
LilyGO (**ESP RX = GPIO 9**), pas de la lecture littérale de la doc matérielle.
⚠️ **Claim corrigé** : « 9600 bauds ⇒ L76K » était **faux**. Le **MAX-M10S sort lui aussi à 9600 par
défaut** (c'est le M9 qui est à 38400) — le débit ne permet donc **pas** de distinguer les deux
variantes. Se fier à la référence d'achat.

## Alimentation de secours du GNSS (démarrage chaud)
Doc matérielle LilyGO, textuel : *« T-Beam Supreme GPS backup power comes from 18650 battery. If you
remove the 18650 battery, you will not be able to get GPS hot start. »*
→ **Ni pile bouton ni supercondensateur** sur cette carte, et le `VBACKUP` de l'AXP2101 est *unused* :
**rien à configurer en logiciel**. Conséquence pratique :

| Configuration | Rallumage |
|---|---|
| 18650 en place (carte « éteinte ») | démarrage **chaud** — fix en quelques secondes |
| USB seul, sans 18650 | démarrage **froid systématique** — 30 s à plusieurs minutes |

⇒ **laisser une 18650 à demeure** dans chaque nœud. Règle aussi l'alimentation de plusieurs cartes en
parallèle pour les tests radio.

## Allumage / extinction
- **Appui long > 6 s sur PWR** = extinction, gérée au niveau **matériel** par l'AXP2101 (fonctionne
  même firmware planté). Allumage : appui long également.
- ⚠️ **Débrancher l'USB avant** : VBUS présent ⇒ le PMU rallume aussitôt la carte.
- ❌ **Ne pas éteindre en retirant la 18650** : c'est la sauvegarde GNSS (cf. ci-dessus) ⇒ démarrage
  froid au retour. Retirer la cellule uniquement pour un **stockage longue durée** (le courant de
  repos du PMU la viderait en quelques semaines).
- ⚠️ **Claim corrigé** : **pas d'interrupteur à glissière** sur la Supreme (contrairement aux T-Beam
  v1.x) — l'alimentation passe uniquement par le bouton PWR du PMU.

**USB** : l'ESP32-S3 s'énumère en **USB natif** (`VID_303A / PID_1001`), pas via un pont USB-UART →
`ARDUINO_USB_CDC_ON_BOOT=1` est le bon réglage. ⚠️ Prévoir un **câble USB-C data** (non fourni) : un
câble de charge ne fait apparaître aucun port COM. Un upload peut échouer une fois si le port CDC est
encore en cours de réénumération — relancer suffit.

**Bus I2C 0 relevé par scan** (03/08/2026) : `0x1C` = magnétomètre **QMC6310**, `0x3C` = **écran OLED**,
`0x77` = **BME280**. La boussole et le baromètre sont donc disponibles et **inutilisés à ce jour** —
pistes : cap magnétique à l'arrêt (le cap GNSS n'existe qu'en mouvement), altitude barométrique.

**Écran : contrôleur SH1106 confirmé** (affichage aligné, vérifié à l'usage), 128×64, adresse `0x3C`,
alimenté par ALDO1.

**GNSS en intérieur** : zéro satellite loin d'une fenêtre, 8 satellites à proximité immédiate d'une
ouverture, sur deux exemplaires identiques au même moment. Comportement **normal** — un signal GNSS ne
traverse ni plancher ni mur porteur. Ne pas conclure à une antenne défectueuse sur cette base.

**Rails AXP2101 — piège n°1** : **ALDO3 = radio**, **ALDO4 = GNSS**, ALDO1/2 = capteurs/écran,
BLDO1 = microSD. Sans les allumer, la carte semble morte. ⚠️ Mapping **différent** de la T-Beam S3
Core (ALDO2 = radio, ALDO3 = GPS) : ne pas recopier le code d'une variante à l'autre.

**TCXO — piège n°2** : le module SX1262 LilyGO utilise un TCXO piloté par **DIO3 en 1,8 V** et
**DIO2 en commutateur d'antenne**. Sans ces deux réglages, l'init réussit mais rien n'est émis.

## ⭐ Variante recommandée : T-Beam **Supreme** 868 (GPS u-blox M10S)
| Critère | **Supreme** (recommandé) | v1.2 (budget) |
|---|---|---|
| MCU | **ESP32-S3**, 16 MB Flash, **8 MB PSRAM** | ESP32 d'origine |
| Radio | **SX1262** | SX1262 |
| GPS | **u-blox MAX-M10S** (multi-constellation, fix rapide) | NEO-6M (basique, lent) |
| OLED | 1.3" intégré | 0.96" (souvent à part) |
| Prix | ~50-60 € | ~30-40 € |

**Pourquoi Supreme** : (1) GPS u-blox M10S bien meilleur (le NEO-6M du v1.2 = même classe faible qui frustrait sur le [[lilygo-t-echo|T-Echo]]) ; (2) ESP32-S3 + PSRAM = **marge pour le fork firmware [[gfsk-sub-ghz|GFSK]]** ; (3) SX1262 requis pour le GFSK ; (4) RNode-compatible (code carte 0xDC en 868).

## Précautions d'achat
- Référence **868 MHz** (pas 915 = US, **pas 433**).
- GPS : **u-blox M10S idéal**, mais **L76K acceptable**. Le L76K (Quectel) est **multi-constellation** (GPS+GLONASS+BeiDou) et **bien supérieur au NEO-6M** du v1.2/T-Echo. Le M10S reste un cran au-dessus (TTFF + conso) mais l'écart est mineur pour l'airsoft. ⚠️ Ne pas confondre L76K (correct) et NEO-6M (faible).
- **`rnodeconf` à jour** (couac historique de flash ESP32-S3 « This chip is ESP32-S3 not ESP32 », désormais résolu).
- Régler la **puissance ≤ +14 dBm** en config pour conformité EU 868 (le SX1262 monte à +22 dBm).

## Revendeurs EU (868)
- **Passion Radio** 🇫🇷 (revendeur officiel LilyGO, FR) — réf vérifiée : *tbeam-sx1262-2823* = **Supreme 3.0 SX1262 868 MHz, ESP32-S3, 8 MB Flash/PSRAM, GPS L76K**, ~**69,52 € TTC** (sans 18650 ni USB-C). ✅ Le bon choix (GPS L76K acceptable, cf. précautions).
  - ⚠️ Réf *sx-1262-433-2825* = **même carte mais 433 MHz** → **à éviter** (mauvaise bande EU).
- **Tinytronics** 🇳🇱 — réf vérifiée **LILYGO-H659** = **Supreme 868 MHz + u-blox MAX-M10S + ESP32-S3 + OLED 1.3"** = ⭐ **la variante idéale** (seule avec le M10S des 3 fiches comparées). Prix à confirmer sur la fiche (~+10 € vs L76K), expédition EU. Recommandé pour le prototype.
- **Bot'n'Roll** 🇵🇹 (réf H659-M, confirme SX1262 + u-blox M10S-00B + 868) — variante M10S.
- **SDRStore** 🇪🇺
À vérifier : **868 MHz** (jamais 433/915). GPS L76K = OK, M10S = idéal. Stock volatil. Prévoir **18650** + câble **USB-C** (non inclus).

## À éviter / alternative
- ❌ **T-Beam 1W (32 dBm)** : dépasse les limites de puissance EU → non conforme.
- 💡 **v1.2 868** = alternative budget pour **tests radio/portée** et **nœuds-relais** ([[fleet-roles]]), pas comme device joueur principal (GPS faible, moins de marge firmware).

## Pourquoi retenu (vs autres devices)
- **Modem pur** idéal : radio + GPS + batterie 18650 + BLE, robuste.
- Pas d'écran/clavier superflu (contrairement au [[lilygo-t-deck-plus]]) puisque l'UI est sur le téléphone.
- GPS plus rapide à fixer que le [[lilygo-t-echo|T-Echo]] (surtout en version Supreme/u-blox).

## Rôle
Joueur et chef d'escouade (le chef ajoute une 2e radio pour le dorsal → [[fleet-roles]]).
Prototype : viser **2-3 Supreme 868** (joueurs) + éventuellement **1-2 v1.2** (relais).

Source : [[2026-06-01_sessions-conception]]
