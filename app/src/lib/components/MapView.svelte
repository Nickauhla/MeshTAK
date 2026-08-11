<script lang="ts">
  import type { Feature } from 'geojson';
  import maplibregl from 'maplibre-gl';
  import 'maplibre-gl/dist/maplibre-gl.css';
  import { onDestroy, onMount } from 'svelte';

  import { circleRing, estimateAccuracy } from '../geo/accuracy.ts';
  import { buildStyle, loadTileSettings } from '../map/tiles.ts';
  import { session, type MarkerEntry, type NodeEntry } from '../state/session.svelte.ts';
  import { markerSpec, markerSvg } from '../tactical/markers.ts';
  import MarkerMenu from './MarkerMenu.svelte';

  let container: HTMLDivElement;
  let map: maplibregl.Map | null = null;
  let ready = $state(false);
  let follow = $state(true);
  const markers = new Map<number, maplibregl.Marker>();
  const tacMarkers = new Map<string, maplibregl.Marker>();
  let meMarker: maplibregl.Marker | null = null;

  const ACC_SOURCE = 'accuracy';
  const STATUS_COLOR = ['#4fb85c', '#e0a12a', '#e0483a', '#a97cf0'];
  const STATUS_LABEL = ['OK', 'TOUCHÉ', 'ÉLIMINÉ', 'AIDE'];

  // Appui long : assez long pour ne pas se déclencher sur un déplacement de
  // carte, assez court pour ne pas donner l'impression que rien ne se passe.
  const LONG_PRESS_MS = 420;
  const MOVE_TOLERANCE_PX = 12;

  /** Menu circulaire ouvert : position à l'écran + point visé sur le terrain. */
  let menu = $state<{ x: number; y: number; lat: number; lon: number } | null>(null);
  /** Point tactique sélectionné, pour la fiche du bas. */
  let selectedKey = $state<string | null>(null);
  let renameDraft = $state('');
  let now = $state(Date.now());

  const selected = $derived(selectedKey ? (session.markers[selectedKey] ?? null) : null);

  /**
   * Peint un marqueur : la couleur du statut est posée sur l'élément racine
   * (`--c`), d'où le losange comme l'étiquette la reprennent.
   */
  function paintMarker(el: HTMLElement, node: NodeEntry): void {
    const acc = estimateAccuracy(node.sats, node.hdop);
    el.style.setProperty('--c', STATUS_COLOR[node.status] ?? '#8a9682');
    el.classList.toggle('low-confidence', acc.quality === 'poor');
    el.innerHTML = `
      <div class="glyph"></div>
      <div class="tag">${node.callsign}${acc.quality === 'good' ? '' : ` <i>${acc.label}</i>`}</div>`;
  }

  function popupHtml(node: NodeEntry): string {
    const acc = estimateAccuracy(node.sats, node.hdop);
    return `<b>${node.callsign}</b><br>
      ${STATUS_LABEL[node.status] ?? '?'} ·
      ${node.battPct === 255 ? '–' : node.battPct + ' %'}<br>
      <span style="color:${acc.color}">${acc.label}</span> ·
      ${node.sats} sat${acc.is2D ? ' · point 2D' : ''}`;
  }

  function paintTacMarker(el: HTMLElement, m: MarkerEntry): void {
    const spec = markerSpec(m.kind);
    if (!spec) return;
    el.style.setProperty('--c', spec.color);
    el.innerHTML = `${markerSvg(m.kind, 30)}<div class="tag">${m.label || spec.short}</div>`;
  }

  function ageLabel(ms: number): string {
    const s = Math.round(ms / 1000);
    if (s < 60) return `${s} s`;
    if (s < 3600) return `${Math.round(s / 60)} min`;
    return `${Math.round(s / 3600)} h`;
  }

  /** Ce qu'il reste à vivre au point — l'information qui décide de s'y fier ou non. */
  function remainLabel(m: MarkerEntry, nowMs: number): string {
    if (m.ttlMin === 0) return 'permanent';
    const left = m.ttlMin * 60000 - (nowMs - m.at);
    return left <= 0 ? 'expiré' : `expire dans ${ageLabel(left)}`;
  }

  onMount(() => {
    map = new maplibregl.Map({
      container,
      style: buildStyle(loadTileSettings()),
      center: [2.3522, 48.8566],
      zoom: 15,
      attributionControl: { compact: true },
    });
    map.addControl(new maplibregl.NavigationControl({ showCompass: true }), 'top-right');
    map.addControl(new maplibregl.ScaleControl({ unit: 'metric' }), 'bottom-left');
    map.on('dragstart', () => (follow = false));
    // La carte bouge : ce n'était pas un appui long, c'était un déplacement.
    map.on('movestart', cancelPress);

    map.on('load', () => {
      // Cercles d'incertitude : des polygones en mètres réels, donc corrects à
      // tous les niveaux de zoom (contrairement à un rayon exprimé en pixels).
      map!.addSource(ACC_SOURCE, {
        type: 'geojson',
        data: { type: 'FeatureCollection', features: [] },
      });
      map!.addLayer({
        id: 'accuracy-fill',
        type: 'fill',
        source: ACC_SOURCE,
        paint: { 'fill-color': ['get', 'color'], 'fill-opacity': 0.12 },
      });
      map!.addLayer({
        id: 'accuracy-line',
        type: 'line',
        source: ACC_SOURCE,
        paint: { 'line-color': ['get', 'color'], 'line-opacity': 0.45, 'line-width': 1 },
      });
      ready = true;
    });

    const clock = setInterval(() => (now = Date.now()), 1000);
    return () => clearInterval(clock);
  });

  onDestroy(() => {
    cancelPress();
    markers.forEach((m) => m.remove());
    markers.clear();
    tacMarkers.forEach((m) => m.remove());
    tacMarkers.clear();
    meMarker?.remove();
    map?.remove();
    map = null;
  });

  // Marqueurs des coéquipiers + cercles d'incertitude.
  $effect(() => {
    const m = map;
    const mates = session.squadMates;
    const me = session.myPosition;
    const status = session.status;
    if (!m || !ready) return;

    const features: Feature[] = [];
    const seen = new Set<number>();

    for (const node of mates) {
      if (node.lat === null || node.lon === null) continue;
      seen.add(node.addr);

      // Le cercle n'apparaît que si l'incertitude dit quelque chose : sur un bon
      // fix, il serait plus petit que le marqueur et n'ajouterait que du bruit.
      const acc = estimateAccuracy(node.sats, node.hdop);
      if (acc.showCircle) {
        features.push({
          type: 'Feature',
          properties: { color: acc.color },
          geometry: { type: 'Polygon', coordinates: [circleRing(node.lat, node.lon, acc.meters)] },
        });
      }

      const existing = markers.get(node.addr);
      if (existing) {
        existing.setLngLat([node.lon, node.lat]);
        paintMarker(existing.getElement(), node);
        existing.getPopup()?.setHTML(popupHtml(node));
      } else {
        const el = document.createElement('div');
        el.className = 'node-marker';
        paintMarker(el, node);
        markers.set(
          node.addr,
          new maplibregl.Marker({ element: el })
            .setLngLat([node.lon, node.lat])
            .setPopup(new maplibregl.Popup({ offset: 16 }).setHTML(popupHtml(node)))
            .addTo(m),
        );
      }
    }

    for (const [id, marker] of markers) {
      if (!seen.has(id)) {
        marker.remove();
        markers.delete(id);
      }
    }

    // Ma propre incertitude compte autant que celle des autres.
    if (me && status) {
      const acc = estimateAccuracy(status.sats, status.hdop);
      if (acc.showCircle) {
        features.push({
          type: 'Feature',
          properties: { color: acc.color },
          geometry: { type: 'Polygon', coordinates: [circleRing(me.lat, me.lon, acc.meters)] },
        });
      }
    }

    const src = m.getSource(ACC_SOURCE) as maplibregl.GeoJSONSource | undefined;
    src?.setData({ type: 'FeatureCollection', features });
  });

  // Points tactiques posés par l'escouade.
  $effect(() => {
    const m = map;
    const entries = session.squadMarkers;
    if (!m || !ready) return;

    const seen = new Set<string>();
    for (const entry of entries) {
      if (!markerSpec(entry.kind)) continue;
      seen.add(entry.key);

      const existing = tacMarkers.get(entry.key);
      if (existing) {
        existing.setLngLat([entry.lon, entry.lat]);
        paintTacMarker(existing.getElement(), entry);
        continue;
      }

      const el = document.createElement('div');
      el.className = 'tac-marker';
      paintTacMarker(el, entry);
      // On relit l'état à l'ouverture : l'élément survit aux mises à jour du
      // point, la capture de `entry` non.
      const key = entry.key;
      el.addEventListener('click', (e) => {
        e.stopPropagation();
        selectedKey = key;
        renameDraft = session.markers[key]?.label ?? '';
      });
      tacMarkers.set(
        entry.key,
        new maplibregl.Marker({ element: el }).setLngLat([entry.lon, entry.lat]).addTo(m),
      );
    }

    for (const [key, marker] of tacMarkers) {
      if (!seen.has(key)) {
        marker.remove();
        tacMarkers.delete(key);
        if (selectedKey === key) selectedKey = null;
      }
    }
  });

  // Marqueur « moi » + recentrage.
  $effect(() => {
    const m = map;
    const me = session.myPosition;
    if (!m || !me) return;

    if (!meMarker) {
      const el = document.createElement('div');
      el.className = 'me-marker';
      meMarker = new maplibregl.Marker({ element: el }).setLngLat([me.lon, me.lat]).addTo(m);
    } else {
      meMarker.setLngLat([me.lon, me.lat]);
    }
    if (follow) m.easeTo({ center: [me.lon, me.lat], duration: 600 });
  });

  // --- Appui long -------------------------------------------------------------
  let pressTimer: ReturnType<typeof setTimeout> | null = null;
  let pressAt: { x: number; y: number } | null = null;

  function cancelPress(): void {
    if (pressTimer !== null) clearTimeout(pressTimer);
    pressTimer = null;
    pressAt = null;
  }

  /** Les contrôles de la carte et les marqueurs gardent leur propre geste. */
  function onOwnFurniture(target: EventTarget | null): boolean {
    return (
      target instanceof Element &&
      target.closest('.tac-marker, .node-marker, .me-marker, .maplibregl-ctrl, .maplibregl-popup') !==
        null
    );
  }

  /** Poser un point qu'on ne peut pas émettre serait un mensonge : personne
      d'autre ne le verrait, et rien ne le dirait sur la carte. */
  const canPlace = $derived(session.inSquad && session.connected);

  function openMenuAt(px: number, py: number): void {
    if (!map || !canPlace) return;
    const point = map.unproject([px, py]);
    menu = { x: px, y: py, lat: point.lat, lon: point.lng };
    selectedKey = null;
    navigator.vibrate?.(15); // Android uniquement ; iOS l'ignore sans erreur
  }

  function handlePointerDown(e: PointerEvent): void {
    if (menu || e.button > 0 || onOwnFurniture(e.target)) return;
    const rect = container.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;
    pressAt = { x: e.clientX, y: e.clientY };
    pressTimer = setTimeout(() => {
      pressTimer = null;
      openMenuAt(px, py);
    }, LONG_PRESS_MS);
  }

  function handlePointerMove(e: PointerEvent): void {
    if (!pressAt) return;
    if (Math.hypot(e.clientX - pressAt.x, e.clientY - pressAt.y) > MOVE_TOLERANCE_PX) cancelPress();
  }

  /** Clic droit au bureau : même menu, sans attendre. */
  function handleContextMenu(e: MouseEvent): void {
    if (onOwnFurniture(e.target) || !canPlace) return;
    e.preventDefault();
    cancelPress();
    const rect = container.getBoundingClientRect();
    openMenuAt(e.clientX - rect.left, e.clientY - rect.top);
  }

  async function place(kind: number): Promise<void> {
    const at = menu;
    menu = null;
    if (!at) return;
    await session.placeMarker(kind, at.lat, at.lon);
  }

  function recenter() {
    const me = session.myPosition;
    if (map && me) {
      follow = true;
      map.easeTo({ center: [me.lon, me.lat], zoom: 16, duration: 500 });
    }
  }

  function fitSquad() {
    const pts = session.squadMates
      .filter((n) => n.lat !== null && n.lon !== null)
      .map((n) => [n.lon as number, n.lat as number] as [number, number]);
    const me = session.myPosition;
    if (me) pts.push([me.lon, me.lat]);
    if (!map || pts.length === 0) return;

    follow = false;
    const bounds = pts.reduce((b, p) => b.extend(p), new maplibregl.LngLatBounds(pts[0], pts[0]));
    map.fitBounds(bounds, { padding: 64, maxZoom: 17, duration: 600 });
  }
</script>

<div
  class="map-wrap"
  onpointerdown={handlePointerDown}
  onpointermove={handlePointerMove}
  onpointerup={cancelPress}
  onpointercancel={cancelPress}
  oncontextmenu={handleContextMenu}
  role="presentation"
>
  <div class="map" bind:this={container}></div>

  <!-- Équerres de cadrage : purement visuelles, elles n'interceptent rien. -->
  <div class="brackets" aria-hidden="true"></div>

  <div class="overlay">
    <button class:active={follow} onclick={recenter}>◎ Moi</button>
    <button onclick={fitSquad}>⛶ Escouade</button>
  </div>

  {#if menu}
    <MarkerMenu x={menu.x} y={menu.y} onpick={place} onclose={() => (menu = null)} />
  {/if}

  {#if selected}
    {@const spec = markerSpec(selected.kind)}
    <div class="sheet" style:--c={spec?.color}>
      <div class="head">
        {@html markerSvg(selected.kind, 26)}
        <div class="ident">
          <strong>{selected.label || spec?.label}</strong>
          <small>
            {spec?.label}
            <span class="sep">·</span>{session.markerAuthor(selected)}
            <span class="sep">·</span>il y a {ageLabel(now - selected.at)}
            <span class="sep">·</span>{remainLabel(selected, now)}
          </small>
        </div>
        <button class="btn-ghost x" onclick={() => (selectedKey = null)}>✕</button>
      </div>
      <div class="acts">
        <input
          bind:value={renameDraft}
          maxlength="20"
          placeholder={spec?.label ?? 'Libellé'}
          onkeydown={(e) => {
            if (e.key === 'Enter') session.renameMarker(selected.key, renameDraft);
          }}
        />
        <button
          class="btn-ghost"
          disabled={renameDraft === selected.label || !session.connected}
          onclick={() => session.renameMarker(selected.key, renameDraft)}
        >
          Nommer
        </button>
        <button class="btn-danger" onclick={() => session.removeMarker(selected.key)}>
          Retirer
        </button>
      </div>
    </div>
  {/if}

  {#if !session.myPosition}
    <div class="banner">
      <span class="pip"></span>
      {session.connected ? 'Acquisition GPS en cours…' : 'T-Beam non connecté'}
    </div>
  {/if}
</div>

<style>
  .map-wrap {
    position: relative;
    flex: 1;
    min-height: 0;
    /* L'appui long doit rester à nous : pas de loupe ni de menu système iOS. */
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    user-select: none;
  }
  .map {
    position: absolute;
    inset: 0;
  }
  /* Cadre de visée : quatre équerres phosphore dans les coins de la carte. */
  .brackets {
    position: absolute;
    inset: 10px;
    pointer-events: none;
    z-index: 4;
    background:
      linear-gradient(var(--accent), var(--accent)) left top / 18px 1px no-repeat,
      linear-gradient(var(--accent), var(--accent)) left top / 1px 18px no-repeat,
      linear-gradient(var(--accent), var(--accent)) right top / 18px 1px no-repeat,
      linear-gradient(var(--accent), var(--accent)) right top / 1px 18px no-repeat,
      linear-gradient(var(--accent), var(--accent)) left bottom / 18px 1px no-repeat,
      linear-gradient(var(--accent), var(--accent)) left bottom / 1px 18px no-repeat,
      linear-gradient(var(--accent), var(--accent)) right bottom / 18px 1px no-repeat,
      linear-gradient(var(--accent), var(--accent)) right bottom / 1px 18px no-repeat;
    opacity: 0.4;
  }

  .overlay {
    position: absolute;
    left: 12px;
    top: 12px;
    display: flex;
    gap: 6px;
    z-index: 5;
  }
  .overlay button {
    background: rgba(8, 11, 8, 0.86);
    backdrop-filter: blur(2px);
    color: var(--muted);
    border: 1px solid var(--line);
    padding: 8px 11px;
    font-size: 11px;
    letter-spacing: 0.12em;
  }
  .overlay button.active {
    border-color: var(--accent);
    color: var(--accent);
    box-shadow: inset 0 0 18px -10px var(--accent);
  }

  /* Fiche d'un point tactique : elle occupe le bas de l'écran, à portée de pouce,
     et laisse le point lui-même visible au-dessus. */
  .sheet {
    position: absolute;
    left: 8px;
    right: 8px;
    bottom: 8px;
    z-index: 6;
    background: rgba(8, 11, 8, 0.95);
    border: 1px solid var(--line);
    border-left: 2px solid var(--c, var(--line));
    border-radius: var(--radius);
    padding: 10px 11px;
    backdrop-filter: blur(3px);
  }
  .head {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .ident {
    flex: 1;
    min-width: 0;
  }
  .ident strong {
    display: block;
    font-size: 14px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .ident small {
    color: var(--muted);
    font-size: 11px;
  }
  .sep {
    color: var(--dim);
    margin: 0 4px;
  }
  .x {
    padding: 4px 9px;
    font-size: 13px;
    flex: none;
  }
  .acts {
    display: flex;
    gap: 6px;
    margin-top: 9px;
  }
  .acts input {
    flex: 1;
    min-width: 0;
    padding: 8px 10px;
    font-size: 13px;
  }
  .acts button {
    flex: none;
    font-size: 11px;
    padding: 8px 11px;
  }

  /* Bandeau d'état : la carte ment tant qu'aucune position n'est acquise, il
     faut que ça se voie. */
  .banner {
    position: absolute;
    bottom: 16px;
    left: 50%;
    transform: translateX(-50%);
    background: rgba(8, 11, 8, 0.92);
    border: 1px solid rgba(224, 161, 42, 0.45);
    color: var(--warn);
    padding: 7px 13px;
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.14em;
    white-space: nowrap;
    display: flex;
    align-items: center;
    gap: 8px;
    z-index: 5;
  }
  .pip {
    width: 6px;
    height: 6px;
    background: var(--warn);
    animation: pip 1.2s steps(2, end) infinite;
  }
  @keyframes pip {
    50% {
      opacity: 0.15;
    }
  }

  /* Marqueurs : éléments HTML, donc aucune police de glyphes à télécharger.
     Losange plein pour un coéquipier, cercle pour soi — deux formes qu'on
     distingue même quand la couleur est mangée par le fond de carte. */
  :global(.node-marker) {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
  }
  :global(.node-marker .glyph) {
    width: 13px;
    height: 13px;
    background: var(--c);
    border: 1px solid rgba(0, 0, 0, 0.6);
    transform: rotate(45deg);
    box-shadow:
      0 0 0 1px rgba(0, 0, 0, 0.85),
      0 0 9px -1px var(--c);
  }
  :global(.node-marker .tag) {
    font-family: var(--font-ui);
    font-size: 10px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    color: var(--fg);
    background: rgba(8, 11, 8, 0.82);
    border-left: 2px solid var(--c);
    padding: 1px 4px;
    white-space: nowrap;
  }
  :global(.node-marker .tag i) {
    font-style: normal;
    font-weight: 400;
    letter-spacing: 0;
    color: var(--muted);
  }
  /* Position douteuse : le marqueur s'efface pour ne pas suggérer une certitude
     que la donnée n'a pas. */
  :global(.node-marker.low-confidence) {
    opacity: 0.6;
  }
  :global(.node-marker.low-confidence .glyph) {
    background: none;
    border: 1.5px dashed var(--c);
    box-shadow: none;
  }
  :global(.me-marker) {
    width: 16px;
    height: 16px;
    border-radius: 50%;
    background: var(--self);
    border: 2px solid #eaf7fb;
    box-shadow:
      0 0 0 1px rgba(0, 0, 0, 0.7),
      0 0 14px rgba(79, 195, 232, 0.85);
  }

  /* Point tactique : le cadre ATAK, plus grand qu'un coéquipier — c'est une
     décision d'équipe, pas une simple position. */
  :global(.tac-marker) {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
    cursor: pointer;
  }
  :global(.tac-marker .mk-glyph) {
    filter: drop-shadow(0 0 3px rgba(0, 0, 0, 0.9));
  }
  :global(.tac-marker .tag) {
    font-family: var(--font-ui);
    font-size: 10px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--fg);
    background: rgba(8, 11, 8, 0.82);
    border-left: 2px solid var(--c);
    padding: 1px 4px;
    max-width: 120px;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
</style>
