<script lang="ts">
  import type { Feature } from 'geojson';
  import maplibregl from 'maplibre-gl';
  import 'maplibre-gl/dist/maplibre-gl.css';
  import { onDestroy, onMount } from 'svelte';

  import { circleRing, estimateAccuracy } from '../geo/accuracy.ts';
  import { buildStyle, loadTileSettings } from '../map/tiles.ts';
  import { session, type NodeEntry } from '../state/session.svelte.ts';

  let container: HTMLDivElement;
  let map: maplibregl.Map | null = null;
  let ready = $state(false);
  let follow = $state(true);
  const markers = new Map<number, maplibregl.Marker>();
  let meMarker: maplibregl.Marker | null = null;

  const ACC_SOURCE = 'accuracy';
  const STATUS_COLOR = ['#4fb85c', '#e0a12a', '#e0483a', '#a97cf0'];
  const STATUS_LABEL = ['OK', 'TOUCHÉ', 'ÉLIMINÉ', 'AIDE'];

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
  });

  onDestroy(() => {
    markers.forEach((m) => m.remove());
    markers.clear();
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

      const acc = estimateAccuracy(node.sats, node.hdop);
      features.push({
        type: 'Feature',
        properties: { color: acc.color },
        geometry: { type: 'Polygon', coordinates: [circleRing(node.lat, node.lon, acc.meters)] },
      });

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
      features.push({
        type: 'Feature',
        properties: { color: acc.color },
        geometry: { type: 'Polygon', coordinates: [circleRing(me.lat, me.lon, acc.meters)] },
      });
    }

    const src = m.getSource(ACC_SOURCE) as maplibregl.GeoJSONSource | undefined;
    src?.setData({ type: 'FeatureCollection', features });
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

<div class="map-wrap">
  <div class="map" bind:this={container}></div>

  <!-- Équerres de cadrage : purement visuelles, elles n'interceptent rien. -->
  <div class="brackets" aria-hidden="true"></div>

  <div class="overlay">
    <button class:active={follow} onclick={recenter}>◎ Moi</button>
    <button onclick={fitSquad}>⛶ Escouade</button>
  </div>

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
</style>
