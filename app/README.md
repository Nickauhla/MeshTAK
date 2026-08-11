# Application MeshRadio — Android + iOS, une seule base de code

Svelte 5 (runes) + TypeScript + **Capacitor 7**. Le même code produit une app Android, une app iOS,
et tourne dans Chrome desktop pour itérer sans build natif.

## Installation

```bash
npm install
```

## Développement

```bash
npm run dev
```

Ouvre l'app dans le navigateur. Chrome desktop expose **Web Bluetooth**, sur lequel le plugin BLE
retombe automatiquement : on peut donc parler à un vrai T-Beam depuis le PC. Safari ne supporte pas
Web Bluetooth — sur iPhone, il faut passer par la build native (ci-dessous), ce qui fonctionne
puisque Capacitor utilise CoreBluetooth et non le navigateur.

```bash
npm test
```

Vérifie le codec du protocole contre les vecteurs partagés avec le firmware (37 tests).

## Builds natives

```bash
npx cap add android && npx cap add ios
```

```bash
npm run android
```

```bash
npm run ios
```

Pour le **live-reload sur téléphone physique** : lancez `npm run dev`, relevez l'IP LAN affichée,
décommentez le bloc `server` de `capacitor.config.ts` avec cette IP, puis `npx cap sync`.

### Permissions

- **Android** — déjà déclarées dans `android/app/src/main/AndroidManifest.xml` : `BLUETOOTH_SCAN`
  avec `neverForLocation` (sans ce drapeau, Android exigerait l'accès à la position) et
  `BLUETOOTH_CONNECT`.
- **iOS** — `NSBluetoothAlwaysUsageDescription`, injectée par la CI (voir ci-dessous).

## iOS depuis Windows, sans Mac

Capacitor ne peut pas compiler pour iOS depuis Windows : Xcode est obligatoire, donc macOS. La
parade, entièrement gratuite :

1. **`.github/workflows/ios.yml`** construit un `.ipa` **non signé** sur un runner macOS de GitHub.
   Aucun certificat ni secret ne transite par GitHub. Onglet *Actions* → *iOS (ipa non signé)* →
   *Run workflow*, puis récupérez l'artefact.
2. **[Sideloadly](https://sideloadly.io/)** (Windows) signe cet `.ipa` avec votre **identifiant Apple
   gratuit** et l'installe sur l'iPhone branché en USB.
3. Sur l'iPhone : *Réglages → Général → VPN et gestion de l'appareil* → faire confiance au
   développeur.

Le Bluetooth ne réclame **aucun droit payant** chez Apple — une déclaration d'usage suffit —, donc un
compte gratuit convient.

**Les limites du compte gratuit**, à connaître avant de s'engager :

| | Compte gratuit | Programme développeur (99 €/an) |
|---|---|---|
| Validité de la signature | **7 jours** | 1 an |
| Apps installées simultanément | 3 | 100+ |
| Distribution | câble USB uniquement | TestFlight |

Prérequis Windows : **iTunes téléchargé depuis apple.com** (la version du Microsoft Store ne fournit
pas les pilotes dont Sideloadly a besoin).

Pour itérer sur l'interface, ne passez pas par là : le navigateur **Bluefy** sur iOS implémente Web
Bluetooth et charge directement le serveur de développement, sans aucune compilation.

## Écrans

| Onglet | Contenu |
|---|---|
| **Carte** | Position de chaque coéquipier, code couleur par statut, **cercle d'incertitude**, **points tactiques** (appui long), suivi automatique, cadrage sur l'escouade |
| **Escouade** | Liste des coéquipiers (distance, **précision estimée**, batterie, fraîcheur, relayé ou direct), points tactiques posés, déclaration de son propre statut, **quitter l'escouade** |
| **Messages** | Diffusion à l'escouade ou message privé avec accusé de réception, messages rapides |
| **Réglages** | Identité, fond de carte, paramètres radio, diagnostic du device |

Avant tout le reste, l'app impose un écran d'**escouade** : créer (on en devient chef) ou demander à
rejoindre. Une demande d'adhésion fait apparaître un bandeau chez le chef, quel que soit l'onglet où
il se trouve, avec l'indicatif du candidat et son **empreinte** — à faire confirmer de vive voix
avant d'accepter, c'est le seul moyen de savoir qu'on valide la bonne personne.

**L'app ne manipule aucune clé.** Le chiffrement vit entièrement dans le firmware : le device
déchiffre et ne transmet que du clair par Bluetooth. C'est ce qui permet d'éviter `crypto.subtle`,
indisponible hors contexte sécurisé — donc absent en développement sur `http://<ip-du-lan>:5173`.

## Architecture

```
src/
  lib/proto/     frames.ts, slip.ts   ← codec, miroir de firmware/include/protocol.h
  lib/ble/       link.ts              ← Nordic UART Service + fragmentation
  lib/state/     session.svelte.ts    ← état global (runes) : flotte, messages, points, config
  lib/map/       tiles.ts             ← fond de carte (OSM / PMTiles / aucun)
  lib/tactical/  markers.ts           ← catalogue ATAK des points (cadres, couleurs, durées)
  lib/components/                     ← les quatre écrans + le menu circulaire
```

`lib/proto` ne dépend de rien d'autre : c'est du TypeScript pur, testé, et **le seul endroit** qui
connaît le format binaire. Toute évolution du protocole part de
[`../shared/PROTOCOL.md`](../shared/PROTOCOL.md), puis `node ../shared/gen-vectors.ts` régénère les
vecteurs que le firmware doit satisfaire.

## Points tactiques

**Appui long sur la carte** (clic droit au bureau) ouvre un **menu circulaire** autour du doigt :
ennemi, véhicule ennemi, ami, contact incertain, objectif, VIP, danger, point de rassemblement. Le
point part chiffré à toute l'escouade et apparaît sur la carte de chacun ; le toucher ouvre une
fiche pour le renommer ou le retirer.

La nomenclature suit **ATAK / MIL-STD-2525** : losange rouge pour un hostile, rectangle bleu pour un
ami, quatrefeuille jaune pour un incertain. La **forme** porte l'information autant que la couleur —
sous soleil rasant ou sur fond de forêt, la couleur seule ne suffit pas.

Deux règles à connaître :

- **Les contacts périment.** Ennemi, ami et incertain disparaissent au bout de 10 minutes ; objectif,
  danger, VIP et rassemblement sont permanents. Une position d'ennemi vue il y a un quart d'heure
  affichée comme fraîche est pire que pas de point du tout.
- **N'importe qui peut retirer n'importe quel point**, pas seulement celui qui l'a posé — sinon le
  point d'un joueur éliminé ou hors de portée resterait à l'écran indéfiniment.

⚠️ Un joueur qui rejoint en cours de partie **ne reçoit pas** les points déjà posés : c'est de la
diffusion, pas de la synchronisation.

## Liaison Bluetooth

L'app **rappelle le boîtier toute seule** quand la liaison tombe (attente croissante de 0,7 à 15 s),
retente immédiatement quand elle revient au premier plan, et **retrouve le T-Beam au lancement
suivant sans passer par le sélecteur**. L'en-tête affiche le numéro d'essai en cours plutôt que de
renvoyer à l'écran de connexion. Seul « Déconnecter le T-Beam » dans les réglages l'oublie pour de
bon.

Poser un point tactique demande une liaison active : l'appui long ne fait rien tant que le boîtier
n'est pas joignable, plutôt que de créer un point que personne d'autre ne verrait.

## Précision des positions

Un point GNSS n'a pas la même valeur selon le nombre de satellites vus. En dessous de **4
satellites**, le récepteur ne résout pas l'altitude : le point est en 2D, et l'erreur horizontale
grimpe. Plutôt que de masquer ces positions — en forêt, « il est par là à 40 m près » vaut mieux que
rien — l'app **affiche l'incertitude** :

- un **cercle** autour du marqueur, en mètres réels (polygone GeoJSON, donc correct à tous les
  zooms), orange ou rouge selon la qualité. Il **n'apparaît pas sur un bon fix** : c'est son
  apparition, puis sa croissance, qui disent « ce point devient douteux » ;
- le rayon estimé (`±12 m`) accolé à l'indicatif dès que la qualité n'est plus bonne ;
- marqueur **estompé et cerclé de pointillés** quand la position est douteuse, mention « point 2D »
  dans la liste d'escouade.

> ⚠️ Le rayon est une **estimation**, pas une mesure : `HDOP × 5 m` d'erreur équivalente utilisateur,
> triplé pour un point 2D (`src/lib/geo/accuracy.ts`). Le récepteur ne transmet aucune erreur réelle.
> À lire comme un ordre de grandeur.

## Cartes hors-ligne

Sur le terrain, il n'y a pas de réseau. Le fond de carte par défaut est OSM en ligne (pratique en
développement) ; pour le terrain, basculez sur **PMTiles** dans les réglages :

1. Générez une archive raster de la zone de jeu (`pmtiles convert zone.mbtiles zone.pmtiles`).
2. Déposez-la dans `public/maps/zone.pmtiles`.
3. Réglages → Fond de carte → PMTiles.

Les marqueurs sont des éléments HTML (pas de police de glyphes MapLibre à télécharger) : l'affichage
des indicatifs fonctionne donc entièrement hors ligne.
