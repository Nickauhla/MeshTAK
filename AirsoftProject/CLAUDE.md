# AirsoftProject — Schéma du wiki (LLM Wiki)

Ce vault Obsidian est une **base de connaissances incrémentale** maintenue par un agent LLM, suivant le pattern « LLM Wiki » (Memex). L'humain source, explore et pose les questions ; le LLM lit, synthétise, classe, relie et tient les pages à jour. **L'humain n'écrit (presque) jamais les pages — le LLM le fait.**

Domaine : conception d'un **dispositif tactique d'airsoft** (GPS + carte + radio mesh LoRa), équivalent civil abordable des systèmes militaires. Voir [[00-overview]].

## Les trois couches

1. **`raw/`** — sources brutes, **immuables**. Le LLM y lit mais ne les modifie jamais. Ici, la source principale est le **digest de nos sessions de conception** (`raw/sources/`). Images dans `raw/assets/`.
2. **`wiki/`** — pages markdown générées par le LLM. Le LLM possède entièrement cette couche : concepts, dispositifs, logiciels, architecture, comparaisons, décisions.
3. **Schéma** — ce fichier `CLAUDE.md`. Il dit comment le wiki est structuré et quels workflows suivre. À faire co-évoluer avec l'usage.

## Arborescence

```
AirsoftProject/
  CLAUDE.md              ← ce schéma
  index.md               ← catalogue (orienté contenu)
  log.md                 ← journal chronologique (append-only)
  raw/sources/           ← sources brutes immuables
  raw/assets/            ← images locales
  wiki/
    00-overview.md       ← point d'entrée / vue d'ensemble
    synthesis.md         ← thèse évolutive (l'état de la pensée)
    concepts/            ← notions (LoRa, Reticulum, mesh, duty-cycle…)
    devices/             ← matériel (T-Beam, T-Deck, T-Echo, WisBlock…)
    software/            ← logiciels (Sideband, RNode, Meshtastic…)
    architecture/        ← architecture système (phone-app, rôles de flotte…)
    comparisons/         ← tableaux comparatifs (Meshtastic vs Reticulum…)
    decisions/           ← journal des décisions (ADR léger)
```

## Conventions de page

- **Frontmatter YAML** sur chaque page : `tags`, `created`, `updated`, `status` (`stub`/`draft`/`stable`), et `type` (`concept`/`device`/`software`/`architecture`/`comparison`/`decision`/`overview`/`source`). Permet d'exploiter Dataview.
- **Wikilinks** : relier généreusement avec `[[nom-de-fichier]]` (sans chemin ni extension — Obsidian résout par nom de fichier, qui est donc unique dans le vault).
- **Citations** : référencer la source via `[[<source>]]` (page de `raw/sources/`) ou un lien web.
- Pages courtes, factuelles, reliées. Une notion = une page. Pas de duplication : si une notion existe, la lier plutôt que la réécrire.
- Marquer explicitement les **contradictions** et les **claims périmés** quand une nouvelle source les supersède.

## Workflows (opérations)

### Ingest (intégrer une source)
1. Lire la source dans `raw/`.
2. Discuter les points clés avec l'humain.
3. Écrire/mettre à jour une page **source** dans `raw/sources/` (résumé) si pertinent.
4. Mettre à jour **toutes les pages concernées** du wiki (concepts, devices, comparaisons, décisions) — une source peut toucher 10-15 pages.
5. Mettre à jour `index.md`.
6. Ajouter une ligne à `log.md` : `## [AAAA-MM-JJ] ingest | <titre>`.

### Query (répondre à une question)
1. Lire `index.md` pour repérer les pages pertinentes, puis les ouvrir.
2. Synthétiser une réponse avec citations.
3. **Filer les bonnes réponses dans le wiki** (nouvelle page comparaison/analyse) — ne pas les laisser disparaître dans le chat.
4. Logguer : `## [AAAA-MM-JJ] query | <sujet>`.

### Lint (vérifier la santé du wiki)
Périodiquement, chercher : contradictions, claims périmés, pages orphelines (sans lien entrant), concepts cités sans page dédiée, cross-références manquantes, trous de données à combler par recherche web. Proposer de nouvelles questions/sources. Logguer le passage.

## Notes
- Le wiki est un repo git de markdown → historique de versions gratuit.
- Vue graphe d'Obsidian = meilleur moyen de voir la forme du wiki (hubs vs orphelins).
- Échelle visée : largement gérable par `index.md` seul (pas besoin de RAG/embeddings ici).
