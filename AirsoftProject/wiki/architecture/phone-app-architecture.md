---
type: architecture
tags: [architecture, ui, ble, chosen]
created: 2026-06-01
updated: 2026-06-02
status: stable
---

# Architecture appli-téléphone (retenue)

**Décision retenue** ([[decision-log]]) : séparer **device** et **UI**.

```
JOUEUR : [ device = RNode radio + GPS ]  ←BLE→  [ téléphone = carte + UI ]
```

Le device est un **modem headless** ([[rnode]]) ; le **téléphone** porte la carte hors-ligne, l'écran tactile et la saisie. C'est l'**architecture native de [[reticulum]]** (RNS tourne sur l'hôte ; la carte LoRa est une interface radio).

## Pourquoi
- L'écran **e-paper** du [[lilygo-t-echo]] rend toute UI embarquée lente → ne pas faire la carte sur le device.
- Le téléphone offre la **meilleure UX cartographique** et existe déjà chez les joueurs.
- C'est ce que fait déjà [[sideband]] (prototype zéro-code).

## Conséquence matérielle
Le device n'a plus besoin d'écran/clavier → **[[lilygo-t-deck-plus]] écarté** (gaspillage), **[[lilygo-t-beam]] retenu** (modem pur, bon GPS, batterie, BLE). Voir [[fleet-roles]].

## Quelle appli ? (prototype vs produit)
**Prototype (maintenant) → [[sideband]]**, sans concurrent : fait déjà tout (BLE→[[rnode]], cartes offline, position, LXMF chiffré sur [[reticulum]]), **zéro code**. Ne rien développer tant que Sideband n'a pas révélé les limites terrain (latence, fix GPS, portée, ergonomie en jeu).

**Produit (après mesure) — 3 chemins, effort croissant :**

| Chemin | Idée | Effort | Cible |
|---|---|---|---|
| **A. Fork de Sideband** | Repartir de Sideband (Python/Kivy + RNS déjà câblé BLE+LXMF+carte), ajouter **trames tactiques** + UI orientée jeu | Faible-moyen | Android + desktop |
| **B. Pont → TAK (CoT)** | Garder RNode/Reticulum en transport, **exposer un flux CoT** (Cursor-on-Target) consommé par **ATAK-CIV** (Android) / **iTAK** (iOS) / **WinTAK** | Moyen-élevé (on n'écrit que le pont) | Multiplateforme / milsim |
| **C. Natif multiplateforme** | App Flutter/React-Native + **port de RNS** | Élevé | iOS+Android custom, produit financé |

> Détail des piles « un seul code, Win/Android/Apple » (Kivy/fork Sideband, BeeWare, Flutter+`rnsd`, cœur Rust) → **[[cross-platform-app-stack]]**.

### Contrainte plateforme (décisive)
- **ATAK-CIV = Android uniquement**, mais l'écosystème TAK couvre **iOS via iTAK** et **Windows via WinTAK** — tous parlent **CoT**.
- ⚠️ Construire le pont (B) comme **flux CoT réseau**, **pas comme plugin ATAK** : le plugin verrouille Android, le flux nourrit **ATAK + iTAK** → multiplateforme gratuit.
- **Le vrai trou iOS, c'est le chemin A** : [[sideband]] = **Android + desktop, pas d'iOS officiel** (Kivy/Python passe mal sur iPhone). iOS est le **point faible structurel** de la pile Reticulum.

**Reco selon le parc téléphones des joueurs :**
- **Mixte iOS/Android** → **chemin B en flux CoT** (couverture max, l'inverse d'une limite).
- **Android dominant / petit budget** → **chemin A** (fork Sideband) + desktop.
- **iOS + UI custom indispensable** → chemin C (cher, réservé au produit financé).

Point invariant : **ne pas reconstruire une carte tactique de zéro** — réutiliser l'UI de Sideband (A) ou de TAK (B).

⚠️ **Licence avant tout fork commercial** : Sideband relève de l'écosystème Reticulum (clauses « no-harm »/« no-AI » signalées sur [[reticulum]]) → usage airsoft = **zone grise** à valider avec l'auteur. **ATAK-CIV / iTAK** sont librement utilisables.

### Axe souveraineté (origine des outils)
Pas d'équivalent **français ouvert et civil** de ATAK-CIV. Les pendants français existent mais sont **gouvernementaux/non publics** : **SICS** (Système d'Information du Combat SCORPION, Eviden — blue-force tracking armée de terre) et **AUXYLIUM** (situational awareness sur smartphone sécurisé, armée de terre). Le « volet civil librement utilisable » est une **spécificité de l'écosystème TAK** (US gov a publié ATAK-CIV + protocole **CoT** ouvert).
- **Chemin B (TAK/CoT)** = meilleure UI mais **origine US gov** (acceptable car libre + protocole ouvert).
- **Chemin A (Reticulum/Sideband)** = écosystème **indépendant non-US** (Mark Qvist, DK) → **mieux placé si la souveraineté est un critère**, et tu maîtrises la pile.
- **Chemin C** = souveraineté totale, coût maximal.

## Réserves assumées
- **Un téléphone par joueur** (coût/logistique ; chest-mount + modem clipsé au gilet).
- Fragilité / batterie / lisibilité au soleil du téléphone en action.
- **Lien BLE** modem↔téléphone à fiabiliser (portée ~10 m, reconnexion auto) → à tester tôt.

Liens : [[hierarchical-segmented-network]], [[reticulum]], [[rnode]]
Source : [[2026-06-01_sessions-conception]]
